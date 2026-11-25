<!-- src/views/StudentDetailView.vue -->
<template>
  <div>
    <!-- Back button -->
    <RouterLink to="/students" class="back-link">← Retour aux étudiants</RouterLink>

    <div v-if="loading" class="loading-state">Chargement...</div>
    <div v-else-if="profileError" class="loading-state">{{ profileError }}</div>
    <div v-else-if="!student" class="loading-state">Étudiant introuvable ou inaccessible.</div>

    <div v-else>
      <!-- Student header -->
      <div class="student-header card">
        <div class="student-avatar" :style="`background:${avatarColor.bg};color:${avatarColor.text}`">
          {{ initials }}
        </div>
        <div class="student-info">
          <div class="student-name">{{ student.name }}</div>
          <div class="student-meta font-mono">
          {{ student.filiere || 'Filière non renseignée' }} · {{ student.grade || 'Niveau non renseigné' }} · Badge : {{ student.badgeUID || 'Non assigné' }}
          </div>
          <div style="margin-top:8px;display:flex;gap:8px">
            <span class="badge" :class="presenceBadge">{{ presenceLabel }}</span>
            <span class="badge" :class="payBadge">{{ payLabel }}</span>
          </div>
        </div>
        <div class="student-actions">
          <button v-if="authStore.isAdmin" class="btn-danger" @click="confirmDelete">Supprimer</button>
        </div>
      </div>

      <div class="detail-grid">
        <!-- Attendance history -->
        <div class="card">
          <div class="card-header">
            <span class="card-title">Historique des présences</span>
            <span class="font-mono" style="font-size:11px;color:#888">
              {{ attendanceRate }}% sur {{ attendances.length }} enregistrements
            </span>
          </div>
          <div class="presence-bar-big">
            <div class="presence-fill-big" :style="`width:${attendanceRate}%`"></div>
          </div>
          <div class="attendance-list">
            <div v-for="a in attendances" :key="a.id" class="attendance-row">
              <span class="att-date font-mono">{{ a.date }}</span>
              <span class="att-course">{{ a.course }}</span>
              <span class="att-time font-mono">{{ a.time }}</span>
              <span class="badge" :class="statusBadge(a.status)">{{ statusLabel(a.status) }}</span>
            </div>
            <div v-if="attendanceError" class="section-error">{{ attendanceError }}</div>
            <div v-if="!attendances.length && !attendanceError" class="empty-message">Aucun historique de présence disponible.</div>
          </div>
        </div>

        <!-- Right column -->
        <div style="display:flex;flex-direction:column;gap:16px">
          <!-- Contact info -->
          <div class="card">
            <div class="card-header"><span class="card-title">Informations</span></div>
            <div class="info-list">
              <div class="info-row">
                <span class="info-key">Email</span>
                <span class="info-val font-mono">{{ student.email }}</span>
              </div>
              <div class="info-row">
                <span class="info-key">Téléphone parent</span>
                <span class="info-val font-mono">{{ student.parentPhone || '—' }}</span>
              </div>
              <div v-if="student.parentName" class="info-row">
                <span class="info-key">Parent / tuteur</span>
                <span class="info-val">{{ student.parentName }}</span>
              </div>
              <div class="info-row">
                <span class="info-key">Filière</span>
                <span class="info-val">{{ student.filiere }}</span>
              </div>
              <div class="info-row">
                <span class="info-key">Badge NFC</span>
                <span class="info-val font-mono">{{ student.badgeUID || 'Non assigné' }}</span>
              </div>
              <div class="info-row">
                <span class="info-key">Statut</span>
                <span class="info-val">{{ student.isActive === false ? 'Inactif' : 'Actif' }}</span>
              </div>
              <div class="info-row">
                <span class="info-key">Inscrit le</span>
                <span class="info-val font-mono">{{ formatDate(student.createdAt) }}</span>
              </div>
            </div>
          </div>

          <!-- Payments -->
          <div v-if="authStore.isAdmin" class="card">
          <div class="card-header"><span class="card-title">Paiements</span></div>
            <div class="pay-list">
              <div v-for="p in payments" :key="p.id" class="pay-row">
                <div>
                <div class="pay-month">{{ p.month || formatDate(p.date) }}</div>
                <div class="pay-method font-mono">{{ p.method || '—' }}</div>
                </div>
                <div style="text-align:right">
                <div class="pay-amount font-mono">{{ p.amount }} MAD</div>
                <span class="badge" :class="p.status === 'paid' ? 'badge-present' : 'badge-absent'">
                  {{ p.status === 'paid' ? 'Payé' : (p.status || 'En attente') }}
                  </span>
                </div>
              </div>
            <div v-if="paymentError" class="section-error">{{ paymentError }}</div>
            <div v-if="!payments.length && !paymentError" class="empty-message">Aucun paiement enregistré.</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onUnmounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { onSnapshot } from 'firebase/firestore'
import { useStudentStore } from '@/stores/studentStore'
import { useAuthStore } from '@/stores/authStore'
import { studentDoc } from '@/firebase/collections'

const route  = useRoute()
const router = useRouter()
const studentStore = useStudentStore()
const authStore = useAuthStore()
const student = ref(null)
const loading = ref(true)
const attendances = ref([])
const payments = ref([])
const profileError = ref('')
const attendanceError = ref('')
const paymentError = ref('')
let unsubscribeStudent = null
let loadVersion = 0

const initials    = computed(() => (student.value?.name || '?').split(' ').map(n=>n[0]).join('').toUpperCase())
const avatarColor = computed(() => {
  const map = { Mathématiques:{bg:'#E1F5EE',text:'#085041'}, PC:{bg:'#E6F1FB',text:'#0C447C'}, SVT:{bg:'#FAEEDA',text:'#633806'} }
  return map[student.value?.filiere] || { bg:'#f0f0ee', text:'#555' }
})
const attendanceRate = computed(() => {
  if (!attendances.value.length) return 0
  const attended = attendances.value.filter(item => ['present', 'present_unpaid', 'late'].includes(item.status)).length
  return Math.round(attended / attendances.value.length * 100)
})
const presenceBadge = computed(() => attendanceRate.value >= 80 ? 'badge-present' : attendanceRate.value >= 60 ? 'badge-late' : 'badge-absent')
const presenceLabel = computed(() => `Présence: ${attendanceRate.value}%`)
const payBadge      = computed(() => ({ paid:'badge-present', pending:'badge-late', overdue:'badge-absent' }[student.value?.paymentStatus] || 'badge-info'))
const payLabel      = computed(() => ({ paid:'Paiement à jour', pending:'Paiement en attente', overdue:'Paiement en retard' }[student.value?.paymentStatus] || 'Statut inconnu'))

const statusBadge = (s) => ({ present:'badge-present', present_unpaid:'badge-late', late:'badge-late', absent:'badge-absent' }[s] || '')
const statusLabel = (s) => ({ present:'Présent', present_unpaid:'Présent · paiement en attente', late:'En retard', absent:'Absent' }[s] || s)

async function confirmDelete() {
  if (!student.value || !confirm(`Supprimer ${student.value.name} ?`)) return
  await studentStore.deleteStudent(student.value.id)
  router.push('/students')
}
function formatDate(value) {
  const date = value?.toDate ? value.toDate() : new Date(value)
  return Number.isNaN(date.getTime()) ? 'Date inconnue' : date.toLocaleDateString('fr-FR')
}

watch(() => route.params.id, async (id) => {
  const version = ++loadVersion
  unsubscribeStudent?.()
  unsubscribeStudent = null
  loading.value = true
  profileError.value = ''
  attendanceError.value = ''
  paymentError.value = ''
  student.value = null
  attendances.value = []
  payments.value = []
  try {
    student.value = await studentStore.fetchStudent(id)
    if (!student.value || version !== loadVersion) return
    unsubscribeStudent = onSnapshot(studentDoc(id), (snapshot) => {
      if (version === loadVersion) student.value = snapshot.exists() ? { id: snapshot.id, ...snapshot.data() } : null
    }, (error) => {
      console.error('Student detail listener failed:', error)
      if (version === loadVersion) profileError.value = 'Impossible de suivre les mises à jour du profil.'
    })
    const historyResults = await Promise.allSettled([
      studentStore.fetchAttendances(id),
      authStore.isAdmin ? studentStore.fetchPayments(id) : Promise.resolve([]),
    ])
    if (version === loadVersion) {
      if (historyResults[0].status === 'fulfilled') {
        attendances.value = historyResults[0].value
      } else {
        console.error('Student attendance history load failed:', historyResults[0].reason)
        attendanceError.value = 'Historique indisponible. Vérifiez l’index Firestore studentId pour attendances et les règles de lecture.'
      }
      if (historyResults[1].status === 'fulfilled') {
        payments.value = historyResults[1].value
      } else {
        console.error('Student payment history load failed:', historyResults[1].reason)
        paymentError.value = 'Paiements indisponibles. Vérifiez les règles Firestore pour les paiements.'
      }
    }
  } catch (error) {
    console.error('Student profile load failed:', error)
    if (version === loadVersion) profileError.value = 'Les informations du profil étudiant n’ont pas pu être chargées.'
  } finally {
    if (version === loadVersion) loading.value = false
  }
}, { immediate: true })

onUnmounted(() => {
  loadVersion++
  unsubscribeStudent?.()
})
</script>

<style scoped>
.back-link { display:inline-flex; align-items:center; gap:6px; font-size:13px; color:#888; text-decoration:none; margin-bottom:16px; }
.back-link:hover { color:#1a1a1a; }
.loading-state { text-align:center; padding:60px; color:#888; }
.section-error { color:#9b3434; font-size:12px; padding:10px 0; }

.card { background:#fff; border:0.5px solid rgba(0,0,0,0.1); border-radius:10px; padding:20px; }
.card-header { display:flex; align-items:center; justify-content:space-between; margin-bottom:16px; }
.card-title  { font-size:13px; font-weight:500; }

.student-header { display:flex; align-items:center; gap:20px; margin-bottom:20px; }
.student-avatar { width:60px; height:60px; border-radius:50%; display:flex; align-items:center; justify-content:center; font-size:20px; font-weight:600; flex-shrink:0; }
.student-info   { flex:1; }
.student-name   { font-size:20px; font-weight:600; margin-bottom:4px; }
.student-meta   { font-size:12px; color:#888; }
.student-actions{ display:flex; gap:8px; flex-shrink:0; }

.btn-secondary  { padding:8px 14px; border:0.5px solid rgba(0,0,0,0.2); border-radius:8px; font-size:13px; background:none; cursor:pointer; font-family:inherit; }
.btn-danger     { padding:8px 14px; background:#FCEBEB; color:#791F1F; border:0.5px solid #F7C1C1; border-radius:8px; font-size:13px; cursor:pointer; font-family:inherit; }
.btn-primary-sm { padding:5px 12px; background:var(--c-brand); color:#fff; border:none; border-radius:6px; font-size:12px; font-weight:500; cursor:pointer; font-family:inherit; }

.detail-grid { display:grid; grid-template-columns:1fr 320px; gap:20px; }

.presence-bar-big { height:6px; background:#f0f0ee; border-radius:3px; margin-bottom:16px; }
.presence-fill-big{ height:100%; background:var(--c-brand); border-radius:3px; transition:width 1s ease; }

.attendance-list { display:flex; flex-direction:column; gap:6px; }
.attendance-row  { display:flex; align-items:center; gap:12px; padding:8px 10px; background:#fafaf8; border-radius:7px; font-size:13px; }
.att-date  { font-size:11px; color:#888; width:80px; flex-shrink:0; }
.att-course{ flex:1; font-weight:500; }
.att-time  { font-size:11px; color:#aaa; width:50px; text-align:right; flex-shrink:0; }

.info-list { display:flex; flex-direction:column; gap:2px; }
.info-row  { display:flex; justify-content:space-between; align-items:center; padding:9px 0; border-bottom:0.5px solid rgba(0,0,0,0.06); }
.info-row:last-child { border-bottom:none; }
.info-key  { font-size:12px; color:#888; }
.info-val  { font-size:12px; font-weight:500; }

.pay-list  { display:flex; flex-direction:column; gap:8px; }
.pay-row   { display:flex; justify-content:space-between; align-items:center; padding:10px 12px; background:#fafaf8; border-radius:8px; }
.pay-month { font-size:13px; font-weight:500; }
.pay-method{ font-size:11px; color:#888; margin-top:2px; }
.pay-amount{ font-size:13px; font-weight:600; color:#085041; margin-bottom:4px; }
.empty-message { color:#888; font-size:12px; padding:12px 0; }
</style>
