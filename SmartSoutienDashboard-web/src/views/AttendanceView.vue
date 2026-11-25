<!-- src/views/AttendanceView.vue -->
<template>
  <div>
    <div class="tabs">
      <button v-for="tab in tabs" :key="tab.key"
        class="tab" :class="{ active: activeTab === tab.key }"
        @click="activeTab = tab.key">{{ tab.label }}</button>
    </div>

    <!-- Live stats -->
    <div class="stats-row">
      <div class="stat-mini stat-green">
        <div class="stat-mini-val">{{ attendanceStore.presentCount }}</div>
        <div class="stat-mini-label">Présents</div>
      </div>
      <div class="stat-mini stat-amber">
        <div class="stat-mini-val">{{ attendanceStore.lateCount }}</div>
        <div class="stat-mini-label">En retard</div>
      </div>
      <div class="stat-mini stat-red">
        <div class="stat-mini-val">{{ attendanceStore.absentCount }}</div>
        <div class="stat-mini-label">Absents</div>
      </div>
      <div class="stat-mini stat-blue">
        <div class="stat-mini-val">{{ attendanceStore.presenceRate }}%</div>
        <div class="stat-mini-label">Taux présence</div>
      </div>
    </div>

    <div class="two-col">
      <div class="card">
        <div class="card-header">
          <span class="card-title">Contrôle NFC — Temps réel</span>
          <div style="display:flex;align-items:center;gap:6px">
            <span class="live-dot" :class="{ 'dot-offline': !attendanceStore.isLive }"></span>
            <span class="font-mono" style="font-size:11px;color:#888">
              {{ attendanceStore.isLive ? 'ESP32 connecté' : 'En attente...' }}
            </span>
          </div>
        </div>

        <div v-if="attendanceStore.loading" class="loading-msg">Connexion Firestore...</div>

        <AttendanceLiveList
          :items="visibleAttendances"
        />
      </div>

      <div style="display:flex;flex-direction:column;gap:16px">
        <div class="card">
          <div class="card-header"><span class="card-title">Salles actives</span></div>
          <div class="rooms-list">
            <div v-for="room in activeRooms" :key="room.id" class="room-card"
              :style="`border-left-color:${room.color}`">
              <div class="room-name">{{ room.subject }} — {{ room.name }}</div>
              <div class="room-meta font-mono">{{ room.deviceStatus }}</div>
              <div style="margin-top:6px">
                <span class="badge badge-present">{{ room.present }} présents</span>
                <span v-if="room.pending" class="badge badge-late" style="margin-left:6px">
                  {{ room.pending }} en attente
                </span>
              </div>
            </div>
          </div>
        </div>

        <div class="card">
          <div class="card-header"><span class="card-title">Marquer manuellement</span></div>
          <div class="manual-form">
            <select v-model="manualStudentId" class="search-input">
              <option value="">Sélectionner un étudiant...</option>
              <option v-for="student in studentStore.students" :key="student.id" :value="student.id">
                {{ student.name }}
              </option>
            </select>
            <div v-if="manualError" class="manual-error">{{ manualError }}</div>
            <div class="status-btns">
              <button class="status-btn status-btn--present" :disabled="manualSaving" @click="markManual('present')">Présent</button>
              <button class="status-btn status-btn--late" :disabled="manualSaving" @click="markManual('late')">En retard</button>
              <button class="status-btn status-btn--absent" :disabled="manualSaving" @click="markManual('absent')">Absent</button>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useAttendanceStore } from '@/stores/attendanceStore'
import { useStudentStore } from '@/stores/studentStore'
import { useDeviceStore } from '@/stores/deviceStore'
import AttendanceLiveList from '@/components/attendance/AttendanceLiveList.vue'

const attendanceStore = useAttendanceStore()
const studentStore = useStudentStore()
const deviceStore = useDeviceStore()
const activeTab   = ref('today')
const manualStudentId = ref('')
const manualError = ref('')
const manualSaving = ref(false)

const tabs = [
  { key:'today',  label:"Aujourd'hui" },
  { key:'absent', label:'Absents' },
]

const visibleAttendances = computed(() => activeTab.value === 'absent'
  ? attendanceStore.todayAttendances.filter(item => item.status === 'absent')
  : attendanceStore.todayAttendances)

const activeRooms = computed(() => deviceStore.devices.map(device => {
  const scans = attendanceStore.todayAttendances.filter(scan => scan.deviceId === device.id)
  return {
    id: device.id,
    subject: device.subject || 'Cours',
    name: device.room || device.id,
    present: scans.filter(scan => ['present', 'present_unpaid'].includes(scan.status)).length,
    pending: scans.filter(scan => scan.status === 'present_unpaid').length,
    color: '#1D9E75',
    deviceStatus: deviceStore.isOnline(device) ? 'Appareil en ligne' : 'Appareil hors ligne',
  }
}))

const courseByFiliere = {
  Math: { courseId: 'cours_math_A', sessionId: 'session_002' },
  PC: { courseId: 'cours_pc_B', sessionId: 'session_001' },
  SVT: { courseId: 'cours_svt_C', sessionId: 'session_001' },
}

async function markManual(status) {
  manualError.value = ''
  const student = studentStore.students.find(s => s.id === manualStudentId.value)
  if (!student) {
    manualError.value = 'Sélectionnez un étudiant.'
    return
  }
  const course = courseByFiliere[student.filiere]
  if (!course) {
    manualError.value = 'Aucun cours configuré pour cette filière.'
    return
  }
  manualSaving.value = true
  try {
    await attendanceStore.markAttendance(course.courseId, course.sessionId, student.id, status, student.name)
    manualStudentId.value = ''
  } catch (error) {
    manualError.value = 'Impossible d’enregistrer cette présence. Vérifiez votre connexion et vos droits.'
    console.error(error)
  } finally {
    manualSaving.value = false
  }
}

onMounted(() => {
  studentStore.listenStudents()
  deviceStore.listenDevices()
  attendanceStore.listenAllToday()
})
onUnmounted(() => {
  studentStore.stopListening()
  deviceStore.stopListening()
  attendanceStore.stopListening()
})
</script>

<style scoped>
.tabs { display:flex; gap:2px; background:#f0f0ee; padding:3px; border-radius:8px; margin-bottom:20px; width:fit-content; }
.tab  { padding:7px 18px; border-radius:6px; font-size:13px; cursor:pointer; border:none; background:none; color:#888; font-family:inherit; transition:all 0.15s; }
.tab.active { background:#fff; color:#1a1a1a; font-weight:500; box-shadow:0 1px 3px rgba(0,0,0,0.08); }
.stats-row { display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:12px; margin-bottom:20px; }
.stat-mini { padding:14px 16px; border-radius:10px; border:0.5px solid rgba(0,0,0,0.08); }
.stat-green { background:#E1F5EE; } .stat-amber { background:#FAEEDA; }
.stat-red   { background:#FCEBEB; } .stat-blue  { background:#E6F1FB; }
.stat-mini-val { font-size:22px; font-weight:600; font-family:'IBM Plex Mono',monospace; line-height:1; }
.stat-green .stat-mini-val { color:#085041; } .stat-amber .stat-mini-val { color:#633806; }
.stat-red   .stat-mini-val { color:#791F1F; } .stat-blue  .stat-mini-val { color:#0C447C; }
.stat-mini-label { font-size:11px; color:#888; margin-top:4px; }
.two-col { display:grid; grid-template-columns:1fr 340px; gap:20px; }
.card    { background:#fff; border:0.5px solid rgba(0,0,0,0.1); border-radius:10px; padding:20px; }
.card-header { display:flex; align-items:center; justify-content:space-between; margin-bottom:16px; }
.card-title  { font-size:13px; font-weight:500; }
.loading-msg { text-align:center; padding:20px; color:#aaa; font-size:13px; }
.rooms-list { display:flex; flex-direction:column; gap:10px; }
.room-card  { padding:12px; border:0.5px solid rgba(0,0,0,0.1); border-radius:8px; border-left-width:3px; }
.room-name  { font-size:13px; font-weight:500; }
.room-meta  { font-size:11px; color:#888; margin-top:2px; }
.manual-form { display:flex; flex-direction:column; gap:10px; }
.search-input { width:100%; padding:9px 12px; border:0.5px solid rgba(0,0,0,0.2); border-radius:8px; font-size:13px; font-family:inherit; outline:none; }
.search-input:focus { border-color:var(--c-brand); }
.manual-error { color:#791F1F; font-size:12px; }
.status-btns { display:flex; gap:8px; }
.status-btn  { flex:1; padding:8px; border-radius:8px; border:0.5px solid; font-size:12px; font-weight:500; cursor:pointer; font-family:inherit; }
.status-btn--present { background:#E1F5EE; color:#085041; border-color:#9FE1CB; }
.status-btn--late    { background:#FAEEDA; color:#633806; border-color:#FAC775; }
.status-btn--absent  { background:#FCEBEB; color:#791F1F; border-color:#F7C1C1; }
.dot-offline { background:#E24B4A !important; animation:none !important; }
</style>
