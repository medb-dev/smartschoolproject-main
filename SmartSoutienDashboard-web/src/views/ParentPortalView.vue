<!-- src/views/ParentPortalView.vue -->
<template>
  <div>
    <div v-if="loading" class="empty-state">Chargement des informations...</div>
    <div v-else-if="!student" class="empty-state">Aucun étudiant n’est associé à ce compte parent.</div>
    <template v-else>
    <div class="parent-header">
      <div class="parent-avatar">{{ initials }}</div>
      <div>
        <div class="parent-name">{{ student.name }}</div>
        <div class="parent-meta font-mono">{{ student.filiere }} · Année Baccalauréat</div>
      </div>
    </div>

    <div class="parent-grid">
      <!-- Attendance -->
      <div class="card">
        <div class="card-header"><span class="card-title">Présences ce mois</span></div>
        <div class="presence-summary">
          <div class="ps-item ps-green"><div class="ps-val">{{ countStatus('present') + countStatus('present_unpaid') }}</div><div class="ps-label">Présent</div></div>
          <div class="ps-item ps-amber"><div class="ps-val">{{ countStatus('late') }}</div><div class="ps-label">Retard</div></div>
          <div class="ps-item ps-red"><div class="ps-val">{{ countStatus('absent') }}</div><div class="ps-label">Absent</div></div>
        </div>
      </div>

      <!-- Payments -->
      <div class="card">
        <div class="card-header"><span class="card-title">Paiements</span></div>
        <div class="payment-list">
            <div v-for="p in payments" :key="p.id" class="pay-row">
            <div class="pay-month font-mono">{{ p.month || formatDate(p.date) }}</div>
            <div class="pay-amount font-mono">{{ p.amount }} MAD</div>
            <span class="badge" :class="p.status === 'paid' ? 'badge-present' : 'badge-absent'">
              {{ p.status === 'paid' ? 'Payé' : (p.status || 'En attente') }}
            </span>
          </div>
          <div v-if="!payments.length" class="empty-state">Aucun paiement enregistré.</div>
        </div>
      </div>

      <!-- Attendance history -->
      <div class="card" style="grid-column:span 2">
        <div class="card-header"><span class="card-title">Historique des présences</span></div>
        <div class="schedule-list">
          <div v-for="a in attendances" :key="a.id" class="schedule-row">
            <div class="schedule-day font-mono">{{ formatDate(a.scannedAt) }}</div>
            <div class="schedule-info">{{ a.room || 'Présence enregistrée' }}</div>
            <span class="badge" :class="a.status === 'present' || a.status === 'present_unpaid' ? 'badge-present' : (a.status === 'late' ? 'badge-late' : 'badge-absent')">{{ a.status }}</span>
          </div>
          <div v-if="!attendances.length" class="empty-state">Aucune présence enregistrée.</div>
        </div>
      </div>
    </div>
    </template>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useAuthStore } from '@/stores/authStore'
import { collectionGroup, getDocs, query, where } from 'firebase/firestore'
import { db } from '@/firebase/config'
import { studentsCol } from '@/firebase/collections'
import { useStudentStore } from '@/stores/studentStore'

const authStore = useAuthStore()
const studentStore = useStudentStore()
const loading = ref(true)
const student = ref(null)
const payments = ref([])
const attendances = ref([])
const initials = computed(() => (student.value?.name || '?').split(' ').map(n=>n[0]).join('').toUpperCase())
const currentMonthAttendances = computed(() => attendances.value.filter(item => {
  const date = item.scannedAt?.toDate ? item.scannedAt.toDate() : new Date(item.scannedAt)
  const now = new Date()
  return !Number.isNaN(date.getTime()) && date.getMonth() === now.getMonth() && date.getFullYear() === now.getFullYear()
}))
const countStatus = status => currentMonthAttendances.value.filter(item => item.status === status).length

function formatDate(value) {
  const date = value?.toDate ? value.toDate() : new Date(value)
  return Number.isNaN(date.getTime()) ? '—' : date.toLocaleDateString('fr-FR')
}

onMounted(async () => {
  try {
    const linked = await getDocs(query(studentsCol(), where('parentId', '==', authStore.user.uid)))
    if (linked.empty) return
    student.value = { id: linked.docs[0].id, ...linked.docs[0].data() }
    payments.value = await studentStore.fetchPayments(student.value.id)
    const attendanceSnapshot = await getDocs(query(collectionGroup(db, 'attendances'), where('studentId', '==', student.value.id)))
    attendances.value = attendanceSnapshot.docs.map(d => ({ id: d.id, ...d.data() }))
      .sort((a, b) => (b.scannedAt?.toMillis?.() || 0) - (a.scannedAt?.toMillis?.() || 0))
  } finally {
    loading.value = false
  }
})
</script>

<style scoped>
.parent-header { display:flex; align-items:center; gap:16px; margin-bottom:24px; padding:20px; background:#fff; border-radius:10px; border:0.5px solid rgba(0,0,0,0.1); }
.parent-avatar { width:52px; height:52px; border-radius:50%; background:#E1F5EE; display:flex; align-items:center; justify-content:center; font-size:18px; font-weight:600; color:#085041; flex-shrink:0; }
.parent-name   { font-size:18px; font-weight:600; }
.parent-meta   { font-size:12px; color:#888; margin-top:3px; }

.parent-grid { display:grid; grid-template-columns:1fr 1fr; gap:16px; }
.card { background:#fff; border:0.5px solid rgba(0,0,0,0.1); border-radius:10px; padding:20px; }
.card-header { display:flex; align-items:center; justify-content:space-between; margin-bottom:16px; }
.card-title  { font-size:13px; font-weight:500; }

.presence-summary { display:flex; gap:10px; }
.ps-item { flex:1; padding:14px; border-radius:8px; text-align:center; }
.ps-green { background:#E1F5EE; } .ps-amber { background:#FAEEDA; } .ps-red { background:#FCEBEB; }
.ps-val { font-size:28px; font-weight:700; font-family:'IBM Plex Mono',monospace; line-height:1; }
.ps-green .ps-val { color:#085041; } .ps-amber .ps-val { color:#633806; } .ps-red .ps-val { color:#791F1F; }
.ps-label { font-size:11px; color:#888; margin-top:4px; }

.payment-list { display:flex; flex-direction:column; gap:8px; }
.pay-row { display:flex; align-items:center; gap:12px; padding:9px 10px; background:#fafaf8; border-radius:7px; }
.pay-month  { flex:1; font-size:12px; color:#555; }
.pay-amount { font-size:13px; font-weight:500; color:#1a1a1a; }

.schedule-list { display:flex; flex-direction:column; gap:8px; }
.schedule-row { display:flex; align-items:center; gap:16px; padding:10px 12px; background:#fafaf8; border-radius:8px; }
.schedule-day { width:32px; font-size:12px; color:#888; flex-shrink:0; }
.schedule-subject { font-size:13px; font-weight:500; }
.schedule-detail  { font-size:11px; color:#888; margin-top:2px; }
.empty-state { text-align:center; color:#888; padding:24px; font-size:13px; }
</style>
