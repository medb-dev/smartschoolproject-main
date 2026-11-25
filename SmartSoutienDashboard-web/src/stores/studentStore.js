// src/stores/studentStore.js
import { defineStore } from "pinia";
import { ref, computed } from "vue";
import {
  getDocs,
  getDoc,
  addDoc,
  updateDoc,
  doc,
  runTransaction,
  query,
  orderBy,
  onSnapshot,
  serverTimestamp,
  collectionGroup,
  where,
} from "firebase/firestore";
import { studentsCol, studentDoc, badgeDoc, paymentsCol } from "@/firebase/collections";
import { db } from "@/firebase/config";

export const useStudentStore = defineStore("students", () => {
  // ── State ──────────────────────────────────────────────────
  const students = ref([]);
  const loading = ref(false);
  const isLive = ref(false);
  const selected = ref(null);
  let unsubscribe = null;

  // ── Getters ────────────────────────────────────────────────
  const totalStudents = computed(() => students.value.length);
  const activeStudents = computed(() =>
    students.value.filter((s) => s.isActive !== false)
  );
  const unpaidStudents = computed(() =>
    students.value.filter((s) => s.paymentStatus !== "paid")
  );
  const byFiliere = computed(
    () => (f) => students.value.filter((s) => s.filiere === f)
  );

  // ── Real-time listener ─────────────────────────────────────
  function listenStudents() {
    stopListening();
    loading.value = true;
    isLive.value = false;

    unsubscribe = onSnapshot(
      query(studentsCol(), orderBy("name")),
      (snap) => {
        loading.value = false;
        isLive.value = true;

        snap.docChanges().forEach((change) => {
          const data = { id: change.doc.id, ...change.doc.data() };
          if (change.type === "added") {
            students.value.push(data);
          }
          if (change.type === "modified") {
            const idx = students.value.findIndex((s) => s.id === change.doc.id);
            if (idx !== -1) students.value[idx] = data;
            else students.value.push(data);
          }
          if (change.type === "removed") {
            students.value = students.value.filter(
              (s) => s.id !== change.doc.id
            );
          }
        });
      },
      (err) => {
        console.warn("Student listener error:", err.message);
        loading.value = false;
        isLive.value = false;
      }
    );
    return unsubscribe;
  }

  // ── CRUD ───────────────────────────────────────────────────
  async function fetchStudent(id) {
    const snap = await getDoc(studentDoc(id));
    selected.value = snap.exists() ? { id: snap.id, ...snap.data() } : null;
    return selected.value;
  }

  async function createStudent(data) {
    const studentRef = doc(studentsCol());
    const badgeUID = normalizeBadge(data.badgeUID);
    await runTransaction(db, async (transaction) => {
      if (badgeUID) {
        const badgeSnapshot = await transaction.get(badgeDoc(badgeUID));
        if (badgeSnapshot.exists() && badgeSnapshot.data().studentId !== studentRef.id) {
          throw new Error("Ce badge est déjà associé à un autre étudiant.");
        }
      }
      transaction.set(studentRef, {
        ...data,
        badgeUID,
        isActive: data.isActive ?? true,
        paymentStatus: data.paymentStatus || "pending",
        presenceRate: data.presenceRate ?? 0,
        createdAt: serverTimestamp(),
      });
      if (badgeUID) transaction.set(badgeDoc(badgeUID), {
        studentId: studentRef.id,
        isActive: true,
        createdAt: serverTimestamp(),
      });
    });
    return studentRef.id;
  }

  async function updateStudent(id, data) {
    const studentRef = studentDoc(id);
    const nextBadgeUID = normalizeBadge(data.badgeUID);
    await runTransaction(db, async (transaction) => {
      const studentSnapshot = await transaction.get(studentRef);
      if (!studentSnapshot.exists()) throw new Error("Étudiant introuvable.");
      const previousBadgeUID = normalizeBadge(studentSnapshot.data().badgeUID);
      if (nextBadgeUID && nextBadgeUID !== previousBadgeUID) {
        const badgeSnapshot = await transaction.get(badgeDoc(nextBadgeUID));
        if (badgeSnapshot.exists() && badgeSnapshot.data().studentId !== id) {
          throw new Error("Ce badge est déjà associé à un autre étudiant.");
        }
      }
      if (previousBadgeUID && previousBadgeUID !== nextBadgeUID) {
        transaction.delete(badgeDoc(previousBadgeUID));
      }
      if (nextBadgeUID) transaction.set(badgeDoc(nextBadgeUID), {
        studentId: id,
        isActive: true,
        updatedAt: serverTimestamp(),
      });
      transaction.update(studentRef, { ...data, badgeUID: nextBadgeUID, updatedAt: serverTimestamp() });
    });
  }

  async function deleteStudent(id) {
    const studentRef = studentDoc(id);
    await runTransaction(db, async (transaction) => {
      const studentSnapshot = await transaction.get(studentRef);
      if (!studentSnapshot.exists()) return;
      const badgeUID = normalizeBadge(studentSnapshot.data().badgeUID);
      if (badgeUID) {
        const badgeSnapshot = await transaction.get(badgeDoc(badgeUID));
        if (badgeSnapshot.exists() && badgeSnapshot.data().studentId === id) {
          transaction.delete(badgeDoc(badgeUID));
        }
      }
      transaction.delete(studentRef);
    });
  }

  // ── Payments ───────────────────────────────────────────────
  async function fetchPayments(studentId) {
    const q = query(paymentsCol(studentId), orderBy("date", "desc"));
    const snap = await getDocs(q);
    return snap.docs.map((d) => ({ id: d.id, ...d.data() }));
  }

  async function fetchAttendances(studentId) {
    const q = query(collectionGroup(db, "attendances"), where("studentId", "==", studentId));
    const snap = await getDocs(q);
    return snap.docs.map((d) => {
      const data = d.data();
      const date = data.scannedAt?.toDate ? data.scannedAt.toDate() : new Date(data.scannedAt);
      return {
        id: d.id,
        date: Number.isNaN(date.getTime()) ? "—" : date.toLocaleDateString("fr-FR"),
        time: Number.isNaN(date.getTime()) ? "—" : date.toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit" }),
        course: data.room || data.courseId || "—",
        status: data.status || "—",
        scannedAt: date,
      };
    }).sort((a, b) => b.scannedAt - a.scannedAt);
  }

  async function addPayment(studentId, paymentData) {
    await addDoc(paymentsCol(studentId), {
      ...paymentData,
      date: serverTimestamp(),
    });
    await syncStudentPaymentStatus(studentId);
  }

  async function updatePayment(studentId, paymentId, paymentData) {
    await updateDoc(doc(paymentsCol(studentId), paymentId), {
      ...paymentData,
      updatedAt: serverTimestamp(),
    });
    await syncStudentPaymentStatus(studentId);
  }

  async function syncStudentPaymentStatus(studentId) {
    const paymentSnapshot = await getDocs(paymentsCol(studentId));
    const monthStatuses = new Map();
    paymentSnapshot.docs.forEach((payment) => {
      const data = payment.data();
      const month = data.month || payment.id;
      const status = data.status || "pending";
      const current = monthStatuses.get(month);
      const priority = { pending: 1, overdue: 2, paid: 3 };
      if (!current || priority[status] > priority[current]) {
        monthStatuses.set(month, status);
      }
    });
    const statuses = [...monthStatuses.values()];
    const paymentStatus = statuses.includes("overdue")
      ? "overdue"
      : statuses.includes("pending")
        ? "pending"
        : "paid";
    await updateDoc(studentDoc(studentId), {
      paymentStatus,
      updatedAt: serverTimestamp(),
    });
  }

  function stopListening() {
    if (unsubscribe) {
      unsubscribe();
      unsubscribe = null;
    }
    isLive.value = false;
    students.value = [];
  }

  function normalizeBadge(value) {
    return typeof value === "string" && value.trim() ? value.trim().toUpperCase() : "";
  }

  return {
    students,
    loading,
    isLive,
    selected,
    totalStudents,
    activeStudents,
    unpaidStudents,
    byFiliere,
    listenStudents,
    fetchStudent,
    createStudent,
    updateStudent,
    deleteStudent,
    fetchPayments,
    fetchAttendances,
    addPayment,
    updatePayment,
    stopListening,
  };
});
