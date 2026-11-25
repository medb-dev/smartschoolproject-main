<!-- src/views/DashboardView.vue -->
<template>
  <div>
    <div class="stats-grid">
      <StatCard label="Étudiants actifs" :value="studentStore.activeStudents.length" change="Données Firestore" trend="neutral" color="green" />
      <StatCard label="Taux de présence" :value="attendanceStore.presenceRate + '%'" change="Aujourd'hui" trend="neutral" color="blue" />
      <StatCard label="Revenus du mois" :value="authStore.isAdmin ? `${formatMoney(monthlyRevenue)} MAD` : '—'" :change="authStore.isAdmin ? 'Paiements confirmés' : 'Réservé administration'" trend="neutral" color="amber" />
      <StatCard label="Paiements en attente" :value="studentStore.unpaidStudents.length" change="Données Firestore" trend="neutral" color="red" />
    </div>

    <div class="main-grid">
      <section class="card live-card">
        <div class="card-header">
          <span class="card-title">Présences en temps réel</span>
          <div class="live-status">
            <span class="live-dot" :class="{ 'dot-offline': !attendanceStore.isLive }"></span>
            <span class="font-mono live-label">{{ attendanceStore.isLive ? 'Firestore Live' : 'En attente...' }}</span>
          </div>
        </div>
        <AttendanceLiveList :items="attendanceStore.todayAttendances" />
      </section>

      <div class="side-column">
        <section class="card chart-card">
          <div class="card-header">
            <span class="card-title">Présence — 7 jours</span>
            <span class="chart-caption">Présences enregistrées</span>
          </div>
          <p v-if="attendanceChartError" class="chart-message chart-error">Données de présence indisponibles.</p>
          <p v-else-if="attendanceChartLoading" class="chart-message">Chargement…</p>
          <template v-else>
            <div class="weekly-chart" role="img" aria-label="Présences et absences des sept derniers jours">
              <div v-for="day in attendanceDays" :key="day.key" class="day-column">
                <span class="day-total">{{ day.total }}</span>
                <div class="bar-track">
                  <span class="bar-segment bar-present" :style="{ height: `${barHeight(day.present, maxDailyAttendance)}px` }"></span>
                  <span class="bar-segment bar-late" :style="{ height: `${barHeight(day.late, maxDailyAttendance)}px` }"></span>
                  <span class="bar-segment bar-absent" :style="{ height: `${barHeight(day.absent, maxDailyAttendance)}px` }"></span>
                </div>
                <span class="day-label">{{ day.label }}</span>
              </div>
            </div>
            <div class="chart-legend">
              <span><i class="legend-present"></i>Présent</span>
              <span><i class="legend-late"></i>Retard</span>
              <span><i class="legend-absent"></i>Absent</span>
            </div>
          </template>
        </section>

        <section class="card chart-card revenue-card">
          <div class="card-header">
            <span class="card-title">Revenus mensuels</span>
            <span v-if="authStore.isAdmin" class="font-mono chart-caption">6 derniers mois</span>
          </div>
          <p v-if="!authStore.isAdmin" class="chart-message">Données financières réservées à l’administration.</p>
          <p v-else-if="revenueError" class="chart-message chart-error">Revenus indisponibles. Vérifiez l’accès Firestore.</p>
          <p v-else-if="revenueLoading" class="chart-message">Chargement…</p>
          <template v-else>
            <div class="revenue-total">{{ formatMoney(monthlyRevenue) }} <span>MAD ce mois</span></div>
            <div class="revenue-chart" role="img" aria-label="Revenus payés par mois sur les six derniers mois">
              <div v-for="month in revenueMonths" :key="month.key" class="month-column">
                <span class="month-value">{{ compactMoney(month.total) }}</span>
                <div class="month-track">
                  <span class="month-bar" :style="{ height: `${barHeight(month.total, maxMonthlyRevenue)}px` }"></span>
                </div>
                <span class="month-label">{{ month.label }}</span>
              </div>
            </div>
            <p v-if="!hasRevenue" class="no-revenue">Aucun paiement payé enregistré sur cette période.</p>
          </template>
        </section>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { collectionGroup, limit, onSnapshot, orderBy, query, where } from 'firebase/firestore'
import { db } from '@/firebase/config'
import { useAttendanceStore } from '@/stores/attendanceStore'
import { useStudentStore } from '@/stores/studentStore'
import { useDeviceStore } from '@/stores/deviceStore'
import { useAuthStore } from '@/stores/authStore'
import StatCard from '@/components/dashboard/StatCard.vue'
import AttendanceLiveList from '@/components/attendance/AttendanceLiveList.vue'

const attendanceStore = useAttendanceStore()
const studentStore = useStudentStore()
const deviceStore = useDeviceStore()
const authStore = useAuthStore()
const attendanceChartLoading = ref(true)
const attendanceChartError = ref(false)
const revenueLoading = ref(true)
const revenueError = ref(false)
const attendanceRecords = ref([])
const paymentRecords = ref([])
let unsubscribeAttendanceChart
let unsubscribeRevenue

function dateKey(date) {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`
}

function makeWeek() {
  const today = new Date()
  today.setHours(0, 0, 0, 0)
  return Array.from({ length: 7 }, (_, index) => {
    const date = new Date(today)
    date.setDate(today.getDate() - 6 + index)
    return {
      key: dateKey(date),
      label: new Intl.DateTimeFormat('fr-FR', { weekday: 'short' }).format(date).replace('.', ''),
      present: 0,
      late: 0,
      absent: 0,
      total: 0,
    }
  })
}

function makeRevenueMonths() {
  const now = new Date()
  return Array.from({ length: 6 }, (_, index) => {
    const date = new Date(now.getFullYear(), now.getMonth() - 5 + index, 1)
    return {
      key: `${date.getFullYear()}-${date.getMonth()}`,
      year: date.getFullYear(),
      month: date.getMonth(),
      label: new Intl.DateTimeFormat('fr-FR', { month: 'short' }).format(date).replace('.', ''),
      total: 0,
    }
  })
}

const attendanceDays = computed(() => {
  const days = makeWeek()
  const daysByKey = new Map(days.map(day => [day.key, day]))
  attendanceRecords.value.forEach(record => {
    const value = record.scannedAt
    const date = value?.toDate ? value.toDate() : new Date(value)
    if (Number.isNaN(date.getTime())) return
    const day = daysByKey.get(dateKey(date))
    if (!day) return
    day.total++
    if (['present', 'present_unpaid'].includes(record.status)) day.present++
    else if (record.status === 'late') day.late++
    else if (record.status === 'absent') day.absent++
  })
  return days
})
const maxDailyAttendance = computed(() => Math.max(1, ...attendanceDays.value.map(day => day.total)))
const revenueMonths = computed(() => {
  const months = makeRevenueMonths()
  const monthsByKey = new Map(months.map(month => [month.key, month]))
  paymentRecords.value.forEach(record => {
    if (record.status !== 'paid') return
    const value = record.date
    const date = value?.toDate ? value.toDate() : new Date(value)
    if (Number.isNaN(date.getTime())) return
    const month = monthsByKey.get(`${date.getFullYear()}-${date.getMonth()}`)
    if (month) month.total += Number(record.amount) || 0
  })
  return months
})
const maxMonthlyRevenue = computed(() => Math.max(1, ...revenueMonths.value.map(month => month.total)))
const monthlyRevenue = computed(() => revenueMonths.value.at(-1)?.total || 0)
const hasRevenue = computed(() => revenueMonths.value.some(month => month.total > 0))

function barHeight(value, max) {
  return value > 0 ? Math.max(3, Math.round((value / max) * 72)) : 0
}

function formatMoney(amount) {
  return new Intl.NumberFormat('fr-FR', { maximumFractionDigits: 0 }).format(amount || 0)
}

function compactMoney(amount) {
  if (amount >= 10000) return `${(amount / 1000).toFixed(0)}k`
  return formatMoney(amount)
}

onMounted(() => {
  studentStore.listenStudents()
  deviceStore.listenDevices()
  attendanceStore.listenAllToday()

  const weekStart = makeWeek()[0].key
  const [year, month, day] = weekStart.split('-').map(Number)
  const startDate = new Date(year, month - 1, day)
  unsubscribeAttendanceChart = onSnapshot(
    query(collectionGroup(db, 'attendances'), where('scannedAt', '>=', startDate)),
    snapshot => {
      attendanceRecords.value = snapshot.docs.map(item => item.data())
      attendanceChartLoading.value = false
      attendanceChartError.value = false
    },
    error => {
      console.error('Weekly attendance chart load failed:', error)
      attendanceChartLoading.value = false
      attendanceChartError.value = true
    }
  )

  if (authStore.isAdmin) {
    unsubscribeRevenue = onSnapshot(
      query(collectionGroup(db, 'payments'), orderBy('date', 'desc'), limit(500)),
      snapshot => {
        paymentRecords.value = snapshot.docs.map(item => item.data())
        revenueLoading.value = false
        revenueError.value = false
      },
      error => {
        console.error('Monthly revenue chart load failed:', error)
        revenueLoading.value = false
        revenueError.value = true
      }
    )
  } else {
    revenueLoading.value = false
  }
})

onUnmounted(() => {
  unsubscribeAttendanceChart?.()
  unsubscribeRevenue?.()
  studentStore.stopListening()
  deviceStore.stopListening()
  attendanceStore.stopListening()
})
</script>

<style scoped>
.stats-grid { display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:12px; margin-bottom:24px; }
.main-grid { display:grid; grid-template-columns:minmax(0,1fr) 320px; gap:20px; }
.side-column { display:flex; flex-direction:column; gap:16px; min-width:0; }
.card { min-width:0; padding:20px; border:0.5px solid rgba(0,0,0,.1); border-radius:10px; background:#fff; }
.card-header { display:flex; align-items:center; justify-content:space-between; gap:8px; margin-bottom:16px; }
.card-title { font-size:13px; font-weight:500; }
.live-status { display:flex; align-items:center; gap:6px; }
.live-label,.chart-caption { color:#888; font:11px 'IBM Plex Mono',monospace; }
.dot-offline { background:#E24B4A !important; animation:none !important; }
.chart-card { padding:17px; }
.chart-caption { white-space:nowrap; }
.weekly-chart,.revenue-chart { display:flex; justify-content:space-between; gap:5px; }
.day-column,.month-column { display:flex; flex:1; min-width:0; flex-direction:column; align-items:center; justify-content:flex-end; }
.day-total,.month-value { height:14px; color:#777; font:9px 'IBM Plex Mono',monospace; white-space:nowrap; }
.bar-track,.month-track { display:flex; width:100%; height:74px; flex-direction:column; justify-content:flex-end; overflow:hidden; border-radius:4px 4px 0 0; background:#f5f5f3; }
.bar-segment { display:block; width:100%; min-height:0; }
.bar-present { background:#1D9E75; }.bar-late { background:#EF9F27; }.bar-absent { background:#E24B4A; }
.day-label,.month-label { margin-top:6px; color:#888; font:9px 'IBM Plex Mono',monospace; text-transform:capitalize; }
.chart-legend { display:flex; justify-content:center; gap:10px; margin-top:13px; color:#777; font-size:9px; }
.chart-legend span { display:flex; align-items:center; gap:4px; white-space:nowrap; }
.chart-legend i { width:7px; height:7px; border-radius:2px; }
.legend-present { background:#1D9E75; }.legend-late { background:#EF9F27; }.legend-absent { background:#E24B4A; }
.revenue-total { margin:-2px 0 11px; color:#085041; font:600 18px 'IBM Plex Mono',monospace; }
.revenue-total span { color:#888; font-size:10px; font-weight:400; }
.month-track { height:53px; }
.month-bar { display:block; width:100%; border-radius:4px 4px 0 0; background:#1D9E75; }
.chart-message,.no-revenue { padding:10px 0; color:#888; font-size:11px; line-height:1.5; }
.chart-error { color:#9b3434; }
.no-revenue { padding-bottom:0; font-size:10px; }
@media(max-width:1050px) { .main-grid { grid-template-columns:1fr; } .side-column { display:grid; grid-template-columns:1fr 1fr; } }
@media(max-width:760px) { .stats-grid { grid-template-columns:repeat(2,minmax(0,1fr)); } .side-column { grid-template-columns:1fr; } }
</style>
