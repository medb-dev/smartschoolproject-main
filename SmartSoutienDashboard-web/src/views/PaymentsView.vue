<!-- src/views/PaymentsView.vue -->
<template>
  <div>
    <div class="stats-grid">
      <StatCard label="Encaissé ce mois" :value="`${monthlyRevenue} MAD`" change="Données Firestore" trend="neutral" color="green" />
      <StatCard label="En attente" :value="pendingCount" change="Paiements enregistrés" trend="neutral" color="amber" />
      <StatCard label="En retard" :value="overdueCount" change="Paiements enregistrés" trend="neutral" color="red" />
      <StatCard label="Taux payé" :value="`${collectionRate}%`" change="Paiements enregistrés" trend="neutral" color="blue" />
    </div>

    <div class="two-col">
      <!-- Payments list -->
      <div class="card">
        <div class="card-header">
          <span class="card-title">Paiements récents</span>
          <div style="display:flex;gap:8px">
            <button class="btn-sm" @click="generateAllInvoices">Factures PDF</button>
            <button class="btn-primary-sm" @click="showPaymentModal = true">+ Enregistrer</button>
          </div>
        </div>
        <table class="pay-table">
          <thead>
            <tr>
              <th>Étudiant</th><th>Montant</th><th>Méthode</th><th>Date</th><th>Statut</th><th></th>
            </tr>
          </thead>
          <tbody>
            <tr v-if="loading"><td colspan="6" class="empty-state">Chargement des paiements...</td></tr>
            <tr v-else-if="loadError"><td colspan="6" class="empty-state">Impossible de charger les paiements.</td></tr>
            <tr v-else-if="!payments.length"><td colspan="6" class="empty-state">Aucun paiement enregistré.</td></tr>
            <tr v-for="p in payments" :key="p.id" class="table-row">
              <td>
                <div class="student-cell">
                  <div class="avatar-xs" :style="`background:${p.bg};color:${p.tc}`">{{ p.init }}</div>
                  {{ p.name }}
                </div>
              </td>
              <td class="font-mono" style="font-weight:500">{{ p.amount }}</td>
              <td><span class="method-tag">{{ p.method }}</span></td>
              <td class="font-mono" style="font-size:11px;color:#888">{{ p.date }}</td>
              <td><span class="badge" :class="p.badgeClass">{{ p.statusLabel }}</span></td>
              <td>
                <button class="action-link" @click="editPayment(p)">Modifier</button>
                <button class="action-link" @click="generateInvoice(p)">PDF</button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <!-- Right column -->
      <div style="display:flex;flex-direction:column;gap:16px">
        <!-- Payment methods -->
        <div class="card">
          <div class="card-header"><span class="card-title">Modes de paiement</span></div>
          <div class="methods-list">
            <div v-for="m in methods" :key="m.label" class="method-row">
              <span style="font-size:13px">{{ m.label }}</span>
              <div style="display:flex;align-items:center;gap:8px">
                <div class="method-bar">
                  <div class="method-fill" :style="`width:${m.pct}%;background:${m.color}`"></div>
                </div>
                <span class="font-mono" style="font-size:11px">{{ m.pct }}%</span>
              </div>
            </div>
          </div>
        </div>

        <!-- Upcoming alerts -->
        <div class="card">
          <div class="card-header"><span class="card-title">Alertes paiement</span></div>
          <div class="alerts-list">
            <div v-for="a in alerts" :key="a.id" class="alert-item" :class="`alert-${a.type}`">
              <div class="alert-name">{{ a.name }}</div>
              <div class="alert-msg font-mono">{{ a.msg }}</div>
            </div>
            <div v-if="!alerts.length" class="empty-state">Aucun paiement en attente ou en retard.</div>
          </div>
        </div>
      </div>
    </div>

    <div v-if="showPaymentModal" class="modal-overlay" @click.self="closePaymentModal">
      <div class="modal">
        <div class="card-header">
          <span class="card-title">{{ editingPayment ? 'Modifier le paiement' : 'Enregistrer un paiement' }}</span>
          <button class="modal-close" @click="closePaymentModal">×</button>
        </div>
        <div class="payment-form">
          <label>Étudiant
            <select v-model="paymentForm.studentId" :disabled="Boolean(editingPayment)">
              <option value="">Sélectionner...</option>
              <option v-for="student in students" :key="student.id" :value="student.id">{{ student.name }}</option>
            </select>
          </label>
          <label>Mois
            <input v-model="paymentForm.month" type="text" placeholder="Novembre 2026" />
          </label>
          <label>Montant (MAD)
            <input v-model.number="paymentForm.amount" type="number" min="0.01" step="0.01" />
          </label>
          <label>Méthode
            <select v-model="paymentForm.method">
              <option value="">Sélectionner...</option>
              <option>Espèces</option><option>Virement</option><option>CMI</option>
            </select>
          </label>
          <label>Statut
            <select v-model="paymentForm.status">
              <option value="paid">Payé</option>
              <option value="pending">En attente</option>
              <option value="overdue">En retard</option>
            </select>
          </label>
          <p v-if="paymentError" class="form-error">{{ paymentError }}</p>
          <div class="modal-actions">
            <button class="btn-sm" @click="closePaymentModal">Annuler</button>
            <button class="btn-primary-sm" :disabled="savingPayment" @click="savePayment">{{ savingPayment ? 'Enregistrement...' : 'Enregistrer' }}</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import jsPDF from 'jspdf'
import autoTable from 'jspdf-autotable'
import { collectionGroup, getDocs, query, orderBy } from 'firebase/firestore'
import { db } from '@/firebase/config'
import { studentsCol } from '@/firebase/collections'
import { useStudentStore } from '@/stores/studentStore'
import StatCard from '@/components/dashboard/StatCard.vue'

const studentStore = useStudentStore()
const payments = ref([])
const students = ref([])
const loading = ref(true)
const loadError = ref(false)
const showPaymentModal = ref(false)
const savingPayment = ref(false)
const paymentError = ref('')
const editingPayment = ref(null)
const paymentForm = ref({ studentId: '', month: '', amount: null, method: '', status: 'paid' })
const paidPayments = computed(() => payments.value.filter(p => p.status === 'paid'))
const pendingCount = computed(() => payments.value.filter(p => p.status === 'pending').length)
const overdueCount = computed(() => payments.value.filter(p => p.status === 'overdue').length)
const collectionRate = computed(() => payments.value.length ? Math.round(paidPayments.value.length / payments.value.length * 100) : 0)
const monthlyRevenue = computed(() => {
  const now = new Date()
  return paidPayments.value
    .filter(p => p.dateValue && p.dateValue.getMonth() === now.getMonth() && p.dateValue.getFullYear() === now.getFullYear())
    .reduce((sum, p) => sum + p.amountValue, 0)
})
const methods = computed(() => {
  const counts = paidPayments.value.reduce((result, p) => {
    const method = p.method || 'Non précisé'
    result[method] = (result[method] || 0) + 1
    return result
  }, {})
  const count = paidPayments.value.length || 1
  return Object.entries(counts).map(([label, value], index) => ({
    label,
    pct: Math.round(value / count * 100),
    color: ['var(--c-brand)', 'var(--c-amber)', 'var(--c-accent)'][index % 3],
  }))
})
const alerts = computed(() => payments.value
  .filter(p => ['pending', 'overdue'].includes(p.status))
  .map(p => ({
    id: p.id,
    name: p.name,
    msg: `${p.month || 'Échéance'} · ${p.status === 'overdue' ? 'En retard' : 'En attente'}`,
    type: p.status === 'overdue' ? 'red' : 'amber',
  })))

function toDate(value) {
  if (!value) return null
  const date = value.toDate ? value.toDate() : new Date(value)
  return Number.isNaN(date.getTime()) ? null : date
}

async function loadPayments() {
  loading.value = true
  loadError.value = false
  try {
    const [paymentSnapshot, studentSnapshot] = await Promise.all([
      getDocs(query(collectionGroup(db, 'payments'), orderBy('date', 'desc'))),
      getDocs(studentsCol()),
    ])
    students.value = studentSnapshot.docs.map(d => ({ id: d.id, ...d.data() }))
    const studentsById = new Map(students.value.map(student => [student.id, student]))
    payments.value = paymentSnapshot.docs.map(d => {
      const data = d.data()
      const studentId = d.ref.parent.parent?.id
      const student = studentsById.get(studentId)
      const name = student?.name || 'Étudiant supprimé'
      const dateValue = toDate(data.date)
      const palette = { Math: ['#E1F5EE', '#085041'], PC: ['#E6F1FB', '#0C447C'], SVT: ['#FAEEDA', '#633806'] }
      const [bg, tc] = palette[student?.filiere] || ['#f0f0ee', '#555']
      return {
        id: d.id,
        studentId,
        name,
        init: name.split(' ').map(part => part[0]).join('').toUpperCase().slice(0, 2),
        bg,
        tc,
        amountValue: Number(data.amount) || 0,
        amount: `${Number(data.amount) || 0} MAD`,
        method: data.method || '—',
        dateValue,
        date: dateValue ? dateValue.toLocaleDateString('fr-FR') : '—',
        month: data.month || '',
        status: data.status || 'pending',
        statusLabel: ({ paid: 'Payé', pending: 'En attente', overdue: 'Retard' })[data.status] || 'Inconnu',
        badgeClass: data.status === 'paid' ? 'badge-present' : (data.status === 'overdue' ? 'badge-absent' : 'badge-late'),
      }
    })
  } catch (error) {
    console.error('Payment load failed:', error)
    loadError.value = true
  } finally {
    loading.value = false
  }
}

onMounted(loadPayments)

function generateInvoice(p) {
  const doc = new jsPDF()
  doc.setFontSize(18)
  doc.text('SmartSoutien — Facture', 14, 22)
  doc.setFontSize(11)
  doc.text(`Étudiant : ${p.name}`, 14, 36)
  doc.text(`Date     : ${p.date}`, 14, 44)
  doc.text(`Méthode  : ${p.method}`, 14, 52)
  autoTable(doc, {
    startY: 62,
    head: [['Description', 'Montant']],
    body: [['Frais de cours mensuel', p.amount]],
  })
  doc.save(`facture-${p.name.replace(' ','-')}.pdf`)
}

function generateAllInvoices() {
  payments.value.filter(p => p.statusLabel === 'Payé').forEach(generateInvoice)
}

function editPayment(payment) {
  editingPayment.value = payment
  paymentForm.value = {
    studentId: payment.studentId,
    month: payment.month,
    amount: payment.amountValue,
    method: payment.method === '—' ? '' : payment.method,
    status: payment.status,
  }
  paymentError.value = ''
  showPaymentModal.value = true
}

function closePaymentModal() {
  showPaymentModal.value = false
  editingPayment.value = null
  paymentError.value = ''
  paymentForm.value = { studentId: '', month: '', amount: null, method: '', status: 'paid' }
}

async function savePayment() {
  paymentError.value = ''
  const amount = Number(paymentForm.value.amount)
  if (!paymentForm.value.studentId || !paymentForm.value.month.trim() || !Number.isFinite(amount) || amount <= 0 || !paymentForm.value.method) {
    paymentError.value = 'Renseignez l’étudiant, le mois, un montant positif et la méthode.'
    return
  }
  savingPayment.value = true
  try {
    const data = {
      month: paymentForm.value.month.trim(),
      amount,
      method: paymentForm.value.method,
      status: paymentForm.value.status,
    }
    if (editingPayment.value) {
      await studentStore.updatePayment(editingPayment.value.studentId, editingPayment.value.id, data)
    } else {
      await studentStore.addPayment(paymentForm.value.studentId, data)
    }
    closePaymentModal()
    await loadPayments()
  } catch (error) {
    console.error('Payment save failed:', error)
    paymentError.value = 'Impossible d’enregistrer le paiement. Vérifiez votre connexion et vos droits.'
  } finally {
    savingPayment.value = false
  }
}
</script>

<style scoped>
.stats-grid { display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:12px; margin-bottom:24px; }
.two-col    { display:grid; grid-template-columns:1fr 300px; gap:20px; }
.card       { background:#fff; border:0.5px solid rgba(0,0,0,0.1); border-radius:10px; padding:20px; }
.card-header{ display:flex; align-items:center; justify-content:space-between; margin-bottom:16px; }
.card-title { font-size:13px; font-weight:500; }
.btn-sm     { padding:6px 12px; border:0.5px solid rgba(0,0,0,0.2); border-radius:6px; font-size:12px; background:none; cursor:pointer; font-family:inherit; }
.btn-primary-sm { padding:6px 12px; background:var(--c-brand); color:#fff; border:none; border-radius:6px; font-size:12px; font-weight:500; cursor:pointer; font-family:inherit; }

.pay-table { width:100%; border-collapse:collapse; }
.pay-table th { font-size:11px; font-family:'IBM Plex Mono',monospace; color:#888; text-align:left; padding:8px 10px; border-bottom:0.5px solid rgba(0,0,0,0.08); text-transform:uppercase; font-weight:500; letter-spacing:0.3px; }
.pay-table td { padding:10px; border-bottom:0.5px solid rgba(0,0,0,0.06); font-size:13px; }
.table-row:last-child td { border-bottom:none; }
.table-row:hover td { background:#fafaf8; }

.student-cell { display:flex; align-items:center; gap:8px; }
.avatar-xs { width:26px; height:26px; border-radius:50%; display:flex; align-items:center; justify-content:center; font-size:10px; font-weight:600; flex-shrink:0; }
.method-tag { font-size:11px; padding:2px 7px; background:#f0f0ee; border-radius:8px; color:#555; font-family:'IBM Plex Mono',monospace; }
.action-link { font-size:12px; color:var(--c-accent); background:none; border:none; cursor:pointer; font-family:inherit; }

.methods-list { display:flex; flex-direction:column; gap:12px; }
.method-row   { display:flex; justify-content:space-between; align-items:center; }
.method-bar   { height:6px; width:80px; background:#f0f0ee; border-radius:3px; }
.method-fill  { height:100%; border-radius:3px; }

.alerts-list  { display:flex; flex-direction:column; gap:8px; }
.alert-item   { padding:11px 12px; border-radius:8px; }
.alert-red    { background:#FCEBEB; }
.alert-amber  { background:#FAEEDA; }
.alert-name   { font-size:13px; font-weight:500; }
.alert-red .alert-name   { color:#791F1F; }
.alert-amber .alert-name { color:#633806; }
.alert-msg    { font-size:11px; margin-top:2px; }
.alert-red .alert-msg    { color:#791F1F; }
.alert-amber .alert-msg  { color:#633806; }
.alert-btn    { margin-top:8px; font-size:11px; background:none; border:none; cursor:pointer; font-family:inherit; font-weight:500; }
.alert-red .alert-btn    { color:#791F1F; }
.alert-amber .alert-btn  { color:#633806; }
.empty-state { text-align:center; color:#888; padding:18px; font-size:13px; }
.modal-overlay { position:fixed; inset:0; z-index:100; display:flex; align-items:center; justify-content:center; padding:20px; background:rgba(0,0,0,.4); }
.modal { width:100%; max-width:440px; padding:20px; border-radius:12px; background:#fff; }
.modal-close { border:0; background:none; color:#888; font-size:20px; cursor:pointer; }
.payment-form { display:flex; flex-direction:column; gap:12px; }
.payment-form label { display:flex; flex-direction:column; gap:5px; color:#555; font-size:12px; }
.payment-form input, .payment-form select { padding:9px 10px; border:1px solid #ddd; border-radius:7px; font:inherit; }
.modal-actions { display:flex; justify-content:flex-end; gap:8px; margin-top:8px; }
.modal-actions button:disabled { opacity:.6; cursor:wait; }
.form-error { color:#791F1F; font-size:12px; }
</style>
