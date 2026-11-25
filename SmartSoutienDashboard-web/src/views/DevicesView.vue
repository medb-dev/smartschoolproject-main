<template>
  <div class="devices-page">
    <section class="summary-grid">
      <div class="summary-card"><span>Appareils</span><strong>{{ deviceStore.devices.length }}</strong></div>
      <div class="summary-card"><span>En ligne</span><strong class="text-green">{{ deviceStore.onlineDevices.length }}</strong></div>
      <div class="summary-card"><span>Hors ligne</span><strong class="text-red">{{ deviceStore.offlineDevices.length }}</strong></div>
      <div class="summary-card"><span>Scans enregistrés</span><strong>{{ deviceStore.totalScans }}</strong></div>
    </section>

    <section class="section-block">
      <div class="section-heading">
        <div>
          <h2>Lecteurs NFC</h2>
          <p>État et dernière activité des appareils connectés à Firestore.</p>
        </div>
        <span class="connection-state" :class="deviceStore.isLive ? 'connected' : 'disconnected'">
          <span class="live-dot" :class="{ 'dot-offline': !deviceStore.isLive }"></span>
          {{ deviceStore.isLive ? 'Synchronisé en direct' : 'Connexion en attente' }}
        </span>
      </div>

      <div v-if="deviceStore.loading" class="empty-card">Chargement des appareils…</div>
      <div v-else-if="!deviceStore.devices.length" class="empty-card">Aucun appareil n’a encore envoyé son statut.</div>
      <div v-else class="devices-grid">
        <article v-for="device in deviceStore.devices" :key="device.id" class="device-card">
          <header class="device-header">
            <div>
              <h3>{{ device.room || device.id }}</h3>
              <p>{{ device.subject || 'Cours non renseigné' }}</p>
            </div>
            <span class="status-pill" :class="deviceStore.isOnline(device) ? 'status-online' : 'status-offline'">
              {{ deviceStore.isOnline(device) ? 'En ligne' : 'Hors ligne' }}
            </span>
          </header>
          <div class="device-id font-mono">{{ device.id }}</div>
          <div class="device-metrics">
            <div><span>Scans</span><strong>{{ device.totalScans || 0 }}</strong></div>
            <div><span>Dernière activité</span><strong>{{ formatLastSeen(device.lastSeen) }}</strong></div>
          </div>
          <footer class="device-details font-mono">
            <span>IP {{ device.ip || '—' }}</span>
            <span>RSSI {{ device.rssi != null ? `${device.rssi} dBm` : '—' }}</span>
            <span>{{ device.firmware || 'Firmware inconnu' }}</span>
          </footer>
        </article>
      </div>
    </section>

    <section class="section-block card">
      <div class="section-heading scan-heading">
        <div>
          <h2>Activité NFC récente</h2>
          <p>Scans reçus aujourd’hui, badges reconnus ou refusés.</p>
        </div>
        <span class="connection-state" :class="attendanceStore.isLive ? 'connected' : 'disconnected'">
          {{ attendanceStore.isLive ? 'En direct' : 'En attente' }}
        </span>
      </div>
      <div class="scan-table-wrap">
        <table class="scan-table">
          <thead><tr><th>Heure</th><th>UID</th><th>Étudiant</th><th>Appareil</th><th>Résultat</th></tr></thead>
          <tbody>
            <tr v-for="scan in displayScans" :key="scan.id">
              <td class="font-mono">{{ formatTime(scan.scannedAt) }}</td>
              <td class="font-mono">{{ scan.uid || scan.badgeUID || '—' }}</td>
              <td>{{ scan.studentName || scan.name || 'Badge inconnu' }}</td>
              <td class="font-mono">{{ scan.deviceId || '—' }}</td>
              <td><span class="result-pill" :class="scan.allowed ? 'result-allowed' : 'result-denied'">{{ scan.allowed ? 'Autorisé' : 'Refusé' }}</span></td>
            </tr>
            <tr v-if="!displayScans.length"><td colspan="5" class="empty-table">Aucun scan reçu aujourd’hui.</td></tr>
          </tbody>
        </table>
      </div>
    </section>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted } from 'vue'
import { useDeviceStore } from '@/stores/deviceStore'
import { useAttendanceStore } from '@/stores/attendanceStore'

const deviceStore = useDeviceStore()
const attendanceStore = useAttendanceStore()

const displayScans = computed(() => [
  ...attendanceStore.recentScans.map(scan => ({ ...scan, uid: scan.badgeUID || '', allowed: true })),
  ...deviceStore.scanLogs.map(scan => ({ ...scan, name: scan.type === 'unknown' ? 'Badge inconnu' : (scan.studentName || '—'), allowed: scan.allowed === true })),
].sort((a, b) => toMillis(b.scannedAt) - toMillis(a.scannedAt)))

function toMillis(value) {
  if (!value) return 0
  const millis = value?.toDate ? value.toDate().getTime() : new Date(value).getTime()
  return Number.isNaN(millis) ? 0 : millis
}

function formatLastSeen(value) {
  if (!value) return 'Jamais'
  const date = value?.toDate ? value.toDate() : new Date(value)
  return Number.isNaN(date.getTime()) ? 'Inconnue' : date.toLocaleString('fr-FR', { day: '2-digit', month: 'short', hour: '2-digit', minute: '2-digit' })
}

function formatTime(value) {
  if (!value) return '—'
  const date = value?.toDate ? value.toDate() : new Date(value)
  return Number.isNaN(date.getTime()) ? '—' : date.toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}

onMounted(() => {
  deviceStore.listenDevices()
  attendanceStore.listenAllToday()
})
onUnmounted(() => {
  deviceStore.stopListening()
  attendanceStore.stopListening()
})
</script>

<style scoped>
.devices-page { display:flex; flex-direction:column; gap:20px; }
.summary-grid { display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:12px; }
.summary-card,.device-card,.card { background:#fff; border:1px solid rgba(0,0,0,.09); border-radius:10px; }
.summary-card { display:flex; flex-direction:column; gap:8px; padding:16px 18px; color:#888; font-size:12px; }
.summary-card strong { color:#1a1a1a; font:600 24px 'IBM Plex Mono',monospace; }
.summary-card .text-green { color:#085041; } .summary-card .text-red { color:#A32D2D; }
.section-block { min-width:0; }
.section-heading { display:flex; justify-content:space-between; align-items:center; gap:16px; margin-bottom:12px; }
.section-heading h2 { font-size:15px; font-weight:600; }
.section-heading p { color:#888; font-size:12px; margin-top:4px; }
.connection-state { display:flex; align-items:center; gap:7px; font:11px 'IBM Plex Mono',monospace; color:#777; white-space:nowrap; }
.connected .live-dot { background:#1D9E75; }
.disconnected .live-dot,.dot-offline { background:#E24B4A !important; animation:none !important; }
.devices-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:12px; }
.device-card { padding:17px; }
.device-header { display:flex; justify-content:space-between; align-items:flex-start; gap:12px; }
.device-header h3 { font-size:14px; font-weight:600; }
.device-header p { margin-top:3px; color:#888; font-size:12px; }
.status-pill,.result-pill { display:inline-flex; align-items:center; border-radius:999px; padding:4px 9px; font-size:11px; white-space:nowrap; }
.status-online,.result-allowed { background:#E1F5EE; color:#085041; }
.status-offline,.result-denied { background:#FCEBEB; color:#791F1F; }
.device-id { margin-top:14px; color:#888; font-size:11px; }
.device-metrics { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-top:15px; }
.device-metrics div { display:flex; flex-direction:column; gap:4px; }
.device-metrics span { color:#888; font-size:11px; }
.device-metrics strong { font-size:13px; font-weight:500; }
.device-details { display:flex; flex-wrap:wrap; gap:7px 12px; margin-top:15px; padding-top:12px; border-top:1px solid #eee; color:#777; font-size:10px; }
.card { padding:20px; }
.scan-heading { margin-bottom:16px; }
.scan-table-wrap { overflow-x:auto; }
.scan-table { width:100%; border-collapse:collapse; text-align:left; }
.scan-table th { padding:9px 10px; border-bottom:1px solid #eee; color:#888; font:11px 'IBM Plex Mono',monospace; text-transform:uppercase; }
.scan-table td { padding:11px 10px; border-bottom:1px solid #f1f1ef; font-size:12px; }
.empty-card,.empty-table { padding:30px; color:#888; text-align:center; font-size:13px; }
@media(max-width:900px) { .devices-grid { grid-template-columns:repeat(2,minmax(0,1fr)); } }
@media(max-width:620px) { .summary-grid { grid-template-columns:repeat(2,minmax(0,1fr)); } .devices-grid { grid-template-columns:1fr; } .section-heading { align-items:flex-start; flex-direction:column; } }
</style>
