// src/stores/deviceStore.js
import { defineStore } from "pinia";
import { ref, computed } from "vue";
import {
  onSnapshot,
  query,
  where,
  collection,
  orderBy,
  updateDoc,
  serverTimestamp,
} from "firebase/firestore";
import { devicesCol, deviceDoc } from "@/firebase/collections";
import { db } from "@/firebase/config";

export const useDeviceStore = defineStore("devices", () => {
  // ── State ──────────────────────────────────────────────────
  const devices = ref([]);
  const scanLogs = ref([]);
  const loading = ref(false);
  const isLive = ref(false);
  const clock = ref(Date.now());
  let unsubscribe = null;
  let unsubscribeScanLogs = null;
  let freshnessTimer = null;

  // ── Getters ────────────────────────────────────────────────
  const onlineDevices = computed(() =>
    devices.value.filter((d) => isOnline(d))
  );
  const offlineDevices = computed(() =>
    devices.value.filter((d) => !isOnline(d))
  );
  const totalScans = computed(() =>
    devices.value.reduce((sum, d) => sum + (d.totalScans || 0), 0)
  );
  const allOnline = computed(
    () => devices.value.length > 0 && offlineDevices.value.length === 0
  );

  function isOnline(device) {
    if (device.status !== "online") return false;
    const lastSeen = device.lastSeen?.toDate ? device.lastSeen.toDate() : new Date(device.lastSeen);
    return !Number.isNaN(lastSeen.getTime()) && clock.value - lastSeen.getTime() <= 90000;
  }

  // ── Real-time listener ─────────────────────────────────────
  function listenDevices() {
    stopListening();
    freshnessTimer = setInterval(() => { clock.value = Date.now(); }, 10000);
    loading.value = true;
    isLive.value = false;

    unsubscribe = onSnapshot(
      query(devicesCol(), orderBy("room")),
      (snap) => {
        loading.value = false;
        isLive.value = true;

        snap.docChanges().forEach((change) => {
          const data = { id: change.doc.id, ...change.doc.data() };

          if (change.type === "added") {
            devices.value.push(data);
          }
          if (change.type === "modified") {
            const idx = devices.value.findIndex((d) => d.id === change.doc.id);
            if (idx !== -1) devices.value[idx] = data;
            else devices.value.push(data);
          }
          if (change.type === "removed") {
            devices.value = devices.value.filter((d) => d.id !== change.doc.id);
          }
        });

        // Sort: online first
        devices.value.sort((a, b) => {
          if (a.status === "online" && b.status !== "online") return -1;
          if (a.status !== "online" && b.status === "online") return 1;
          return 0;
        });
      },
      (err) => {
        console.warn("Device listener error:", err.message);
        loading.value = false;
        isLive.value = false;
      }
    );

    const today = new Date();
    today.setHours(0, 0, 0, 0);
    unsubscribeScanLogs = onSnapshot(
      query(collection(db, "scan_logs"), where("scannedAt", ">=", today)),
      (snap) => {
        scanLogs.value = snap.docs.map((item) => ({ id: item.id, ...item.data() }));
      },
      (err) => console.warn("Scan log listener error:", err.message)
    );
    return unsubscribe;
  }

  // ── Update device config ───────────────────────────────────
  async function updateDevice(id, data) {
    await updateDoc(deviceDoc(id), {
      ...data,
      updatedAt: serverTimestamp(),
    });
  }

  // ── Mark device online/offline manually ───────────────────
  async function setDeviceStatus(id, status) {
    await updateDoc(deviceDoc(id), {
      status,
      lastSeen: serverTimestamp(),
    });
  }

  function stopListening() {
    if (freshnessTimer) {
      clearInterval(freshnessTimer);
      freshnessTimer = null;
    }
    if (unsubscribe) {
      unsubscribe();
      unsubscribe = null;
    }
    if (unsubscribeScanLogs) {
      unsubscribeScanLogs();
      unsubscribeScanLogs = null;
    }
    isLive.value = false;
    devices.value = [];
    scanLogs.value = [];
  }

  return {
    devices,
    scanLogs,
    loading,
    isLive,
    onlineDevices,
    offlineDevices,
    totalScans,
    allOnline,
    isOnline,
    listenDevices,
    updateDevice,
    setDeviceStatus,
    stopListening,
  };
});
