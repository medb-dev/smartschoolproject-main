# SmartSoutien

SmartSoutien is a school-support management system with a Vue dashboard, Firebase/Firestore backend, and ESP32 NFC attendance reader. It manages students, attendance, payments, and connected readers. **There is no AI analytics module in this project.**

## Features

- Student profiles, NFC badge assignments, and payment status
- Live attendance from the ESP32 and manual attendance entry
- Payment recording, editing, summaries, and PDF receipts
- Device online status, heartbeat, scan history, and offline scan queue
- Firebase Authentication and role-based Firestore access
- Dashboard charts for attendance over the last seven days and paid revenue over the last six months

## Project layout

```text
smartschoolproject-main/
├── SmartSoutienDashboard-web/   Vue 3 dashboard and Firebase configuration
│   ├── src/                     Views, stores, router, and Firebase client
│   ├── firestore.rules           Firestore security rules
│   ├── firestore.indexes.json    Firestore indexes and field overrides
│   └── firebase.json             Hosting, rules, and indexes configuration
├── ESP32_SmartSoutien/           Arduino ESP32 NFC firmware and wiring guide
├── .github/workflows/            Firebase Hosting GitHub Actions workflows
└── .gitignore                    Ignores local secrets and generated files
```

## Requirements

- Node.js and npm
- A Firebase project with Firestore (default database), Email/Password Authentication, and Hosting
- Arduino IDE with ESP32 board support and the libraries listed in the firmware README
- ESP32, MFRC522 NFC reader, and compatible NFC badges

## Run the dashboard locally

```powershell
cd SmartSoutienDashboard-web
npm ci
npm run dev
```

Vite prints the local development URL in the terminal. Other useful commands:

```powershell
npm run lint
npm run build
npm run preview
```

The Firebase web client configuration is in `SmartSoutienDashboard-web/src/firebase/config.js`. Firebase web API keys identify the client application; Firestore rules and Authentication must protect data access. Never put service-account private keys, device passwords, or Wi-Fi passwords in the web client or commit them to Git.

## Firebase setup

The checked-in configuration targets Firebase project **`smartsoutien`** and its default Firestore database. To use another project, update the web client config, `SmartSoutienDashboard-web/.firebaserc`, the firmware project ID, and the GitHub Actions project ID consistently.

1. Enable Firestore Database (default database), Email/Password Authentication, and Firebase Hosting.
2. Create Firebase Authentication accounts for staff, parents, and the ESP32. Add a matching `users/{uid}` document for each account. Set `role` to `admin`, `teacher`, `parent`, or `device` as appropriate.
3. For a parent account, set the matching student's `parentId` to that account's Firebase Auth UID.
4. Deploy the rules and indexes from the web project folder:

   ```powershell
   cd SmartSoutienDashboard-web
   npm install --global firebase-tools
   firebase login
   firebase deploy --only firestore:rules,firestore:indexes
   ```

   If Firestore reports a missing index, open its **create index** link and wait for the index to become enabled. In particular, student attendance history filters the `attendances` collection group by `studentId`.

### Firestore data used by the app

```text
users/{uid}
students/{studentId}
students/{studentId}/payments/{paymentId}
badges/{badgeUID}
devices/{deviceId}
courses/{courseId}/sessions/{sessionId}/attendances/{attendanceId}
scan_logs/{scanLogId}
```

Attendance documents use a Firestore `Timestamp` in `scannedAt`, along with fields such as `studentId`, `studentName`, `status`, `deviceId`, and `source`. Payment documents are nested under the student and include `month`, `amount`, `method`, `status`, and `date`.

## Configure and flash the ESP32

See [`ESP32_SmartSoutien/README.md`](ESP32_SmartSoutien/README.md) for wiring, Arduino IDE setup, and Firebase account configuration.

1. Copy `ESP32_SmartSoutien/secrets.example.h` to `ESP32_SmartSoutien/secrets.h` and enter the device's Wi-Fi and Firebase Authentication credentials. `secrets.h` is ignored by Git.
2. Verify `DEVICE_ID`, `COURSE_ID`, and `SESSION_ID` in `ESP32_SmartSoutien/config.h` match the corresponding Firestore records and the device user's `users/{uid}` fields.
3. Open `ESP32_SmartSoutien/ESP32_SmartSoutien.ino` in Arduino IDE, select **ESP32 Dev Module**, install the required libraries, and upload the sketch.
4. Open Serial Monitor at **115200 baud**. A successful scan reports the badge UID, access result, and attendance write result.

The firmware buffers scans locally while offline and retries them after connectivity returns. Accurate attendance timestamps require the ESP32 to synchronize its clock over NTP.

## Deploy the dashboard

To deploy manually after installing and signing in to Firebase CLI:

```powershell
cd SmartSoutienDashboard-web
npm ci
npm run build
firebase deploy --only hosting
```

GitHub Actions deploys Hosting when a commit is pushed to the `main` branch. Add the Firebase service-account JSON as the repository secret `FIREBASE_SERVICE_ACCOUNT_SMARTSOUTIEN`. Firestore rules and indexes are deployed separately with the command above; the Hosting workflow does not deploy them.

## Security notes

- Never commit `ESP32_SmartSoutien/secrets.h`, Wi-Fi credentials, device passwords, or Firebase service-account private keys.
- The root `.gitignore` excludes local firmware secrets, service-account files, dependencies, and build output.
- Use the provided Firestore rules and assign least-privilege roles. The device account should have `role: "device"` plus its `deviceId`, `courseId`, and `sessionId` fields.
- Rotate any credentials that were previously shared or committed.

## GitHub

Clone the repository using its GitHub URL:

```powershell
git clone <your-github-repository-url>
cd <repository-folder>
```

If you downloaded the project as a ZIP, that copy has no Git history or remote configured. Use the original Git clone or connect this folder to the correct GitHub repository before pushing.
