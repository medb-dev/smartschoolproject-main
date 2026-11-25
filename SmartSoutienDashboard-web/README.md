# SmartSoutien — smartschoolproject

Système de gestion intelligente pour école de soutien scolaire (niveau Bac).
Intégration **Vue.js 3 + Firebase + ESP32 NFC + IA Python**.

---

## 🚀 Installation en 5 étapes

### Étape 1 — Cloner et installer les dépendances

```bash
# Cloner le projet
git clone https://github.com/votre-org/smartschoolproject.git
cd smartschoolproject/SmartSoutienDashboard-web

# Installer les dépendances
npm install
```

### Étape 2 — Créer le projet Firebase

1. Aller sur [https://console.firebase.google.com](https://console.firebase.google.com)
2. Utiliser le projet Firebase `smartsoutien` configuré dans le dépôt. Si vous en choisissez un autre, mettez à jour les IDs de projet dans la configuration web, le firmware, `.firebaserc` et les workflows GitHub.
3. Activer les services suivants :
   - **Firestore Database** → Base par défaut
   - **Authentication** → Email/Password
   - **Hosting**
4. **Project Settings** → **"Add app"** → Web (icône `</>`)
5. Déployer les règles du dépôt avec `firebase deploy --only firestore:rules`. Ne pas utiliser les règles de test en production.

### Étape 3 — Configurer Firebase dans le projet

Vérifier que `src/firebase/config.js` correspond à l'application web créée. Les clés de configuration Firebase du client sont publiques; les règles Firestore protègent les données.

### Étape 4 — Créer les premiers utilisateurs Firebase

Dans Firebase Console → **Authentication** → **Add user** :

| Email choisi dans Firebase Authentication | Rôle Firestore |
|--------------------------------------------|----------------|
| Adresse de l'administrateur                | admin          |
| Adresse du professeur                      | teacher        |
| Adresse du parent                          | parent         |

Choisir des mots de passe uniques et ne pas les publier.

Ensuite dans **Firestore** → créer la collection `users` :

```
users/
  {uid_admin}/
    name: "Mohammed Alami"
    email: "admin@soutien.ma"
    role: "admin"
  
  {uid_prof}/
    name: "Prof. Benali"
    email: "prof@soutien.ma"
    role: "teacher"
  
  {uid_parent}/
    name: "Hassan Benali"
    email: "adresse-parent"
    role: "parent"
```

Pour associer le parent, renseigner `parentId: "uid_parent"` sur son document étudiant.

### Étape 5 — Lancer le projet

```bash
npm run dev
# → http://localhost:3000
```

---

## 📁 Structure du projet

```
smartschoolproject/
├── src/
│   ├── firebase/
│   │   ├── config.js          ← Configuration Firebase (à compléter)
│   │   └── collections.js     ← Références Firestore centralisées
│   │
│   ├── stores/                ← Pinia stores (état global)
│   │   ├── authStore.js       ← Authentification + rôles
│   │   ├── studentStore.js    ← CRUD étudiants + paiements
│   │   ├── attendanceStore.js ← Présences temps réel
│   │   └── deviceStore.js     ← Monitoring ESP32
│   │
│   ├── views/                 ← Pages principales
│   │   ├── AppLayout.vue      ← Sidebar + Topbar
│   │   ├── LoginView.vue      ← Connexion
│   │   ├── DashboardView.vue  ← Vue d'ensemble
│   │   ├── AttendanceView.vue ← Présences NFC
│   │   ├── StudentsView.vue   ← Liste étudiants
│   │   ├── PaymentsView.vue   ← Paiements + Factures
│   │   ├── DevicesView.vue    ← ESP32 monitoring
│   │   ├── AnalyticsView.vue  ← IA + Rapports
│   │   └── ParentPortalView.vue ← Espace parent
│   │
│   ├── components/
│   │   ├── dashboard/
│   │   │   ├── StatCard.vue
│   │   │   ├── PresenceWeekChart.vue
│   │   │   └── RevenueSparkline.vue
│   │   └── attendance/
│   │       └── AttendanceLiveList.vue
│   │
│   ├── router/index.js        ← Routes + Auth Guards
│   ├── composables/
│   │   └── useFirestore.js    ← Composables réactifs Firestore
│   ├── locales/
│   │   ├── fr.js              ← Français
│   │   └── ar.js              ← Arabe
│   ├── assets/main.css        ← Styles globaux + Tailwind
│   ├── App.vue
│   └── main.js
│
├── firestore.rules             ← Règles de sécurité Firestore
├── tailwind.config.js
├── vite.config.js
└── package.json
```

---

## 🔌 Intégration ESP32

Le firmware ESP32 est dans le dossier `../ESP32_SmartSoutien/` et envoie les scans NFC à Firestore via HTTPS.

**Collection cible :** `badges/{UID}` → vérifie l'étudiant → enregistre dans `attendances/`

Voir `ESP32_SmartSoutien.ino` et son README pour l'installation.

---

Le module IA est annoncé dans l'ancien plan du projet, mais son code n'est pas présent dans ce dépôt.

---

## 🚢 Déploiement Firebase Hosting

```bash
npm install -g firebase-tools
firebase login
npm run build
firebase deploy --only hosting,firestore
# → https://smartsoutien.web.app
```

For GitHub Actions deployment, configure the repository secret `FIREBASE_SERVICE_ACCOUNT_SMARTSOUTIEN` with a service account for the `smartsoutien` Firebase project.

---

## 🛠 Scripts disponibles

```bash
npm run dev      # Serveur de développement (port 3000)
npm run build    # Build de production
npm run preview  # Prévisualiser le build
npm run lint     # Linter ESLint
```

---

## 👥 Équipe

| Membre | Filière | Module |
|--------|---------|--------|
| Étudiant 1 | Ingénierie Logicielle | Vue.js + Firebase |
| Étudiant 2 | Systèmes Embarqués | ESP32 + NFC |
| Étudiant 3 | Data & IA | Python + ML |
