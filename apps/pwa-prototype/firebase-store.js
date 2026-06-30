const firebaseConfig = {
  apiKey: "AIzaSyAF_WBUeMoqmyIY4YzepdF7WFHjgYy2Rms",
  authDomain: "lifetodo-47399.firebaseapp.com",
  projectId: "lifetodo-47399",
  storageBucket: "lifetodo-47399.firebasestorage.app",
  messagingSenderId: "630925592027",
  appId: "1:630925592027:web:c49732e1060c243c3f53fe",
  measurementId: "G-5Z09SBNKH0"
};

const firebaseVersion = "10.14.1";
const homeId = new URLSearchParams(location.search).get("home") || "demo-home";
const localKey = `lifetodo.${homeId}`;

let firestoreApi;
let authApi;
let db;
let user;

export async function createStore(seed) {
  const local = createLocalStore(seed);

  try {
    const [appModule, authModule, firestoreModule] = await Promise.all([
      import(`https://www.gstatic.com/firebasejs/${firebaseVersion}/firebase-app.js`),
      import(`https://www.gstatic.com/firebasejs/${firebaseVersion}/firebase-auth.js`),
      import(`https://www.gstatic.com/firebasejs/${firebaseVersion}/firebase-firestore.js`)
    ]);

    const app = appModule.initializeApp(firebaseConfig);
    authApi = authModule;
    firestoreApi = firestoreModule;
    db = firestoreApi.getFirestore(app);
    user = await ensureUser();

    const cloud = await readCloudState();
    if (!cloud) {
      await writeCloudState(local.getState(), "seed");
    } else {
      local.replaceState(cloud);
    }

    return createCloudStore(local);
  } catch (error) {
    console.warn("Firebase unavailable, using localStorage.", error);
    return local;
  }
}

function createLocalStore(seed) {
  let state = readLocalState(seed);

  return {
    mode: "local",
    getState() {
      return state;
    },
    replaceState(nextState) {
      state = normalizeState(nextState);
      writeLocalState(state);
    },
    async addTask(task) {
      state.tasks.push(task);
      writeLocalState(state);
    },
    async toggleCompletion(taskId, date, source) {
      toggleLocalCompletion(state, taskId, date, source);
      writeLocalState(state);
    }
  };
}

function createCloudStore(local) {
  return {
    mode: "firebase",
    getState: local.getState,
    replaceState: local.replaceState,
    async addTask(task) {
      local.getState().tasks.push(task);
      await writeCloudState(local.getState(), "addTask");
      writeLocalState(local.getState());
    },
    async toggleCompletion(taskId, date, source) {
      toggleLocalCompletion(local.getState(), taskId, date, source);
      await writeCloudState(local.getState(), source);
      writeLocalState(local.getState());
    }
  };
}

async function ensureUser() {
  if (authApi.getAuth().currentUser) {
    return authApi.getAuth().currentUser;
  }
  const credential = await authApi.signInAnonymously(authApi.getAuth());
  return credential.user;
}

async function readCloudState() {
  const homeRef = firestoreApi.doc(db, "homes", homeId);
  const snapshot = await firestoreApi.getDoc(homeRef);
  if (!snapshot.exists()) {
    return null;
  }
  const data = snapshot.data();
  return normalizeState({
    members: data.members || [],
    tasks: data.tasks || [],
    completions: data.completions || {}
  });
}

async function writeCloudState(state, source) {
  const homeRef = firestoreApi.doc(db, "homes", homeId);
  await firestoreApi.setDoc(
    homeRef,
    {
      name: "家",
      timezone: "Asia/Shanghai",
      updatedBy: user.uid,
      updatedSource: source,
      updatedAt: firestoreApi.serverTimestamp(),
      members: state.members,
      tasks: state.tasks,
      completions: state.completions
    },
    { merge: true }
  );
}

function readLocalState(seed) {
  const stored = localStorage.getItem(localKey);
  if (!stored) {
    return structuredClone(seed);
  }
  try {
    return normalizeState(JSON.parse(stored));
  } catch {
    return structuredClone(seed);
  }
}

function writeLocalState(state) {
  localStorage.setItem(localKey, JSON.stringify(state));
}

function normalizeState(state) {
  return {
    members: Array.isArray(state.members) ? state.members : [],
    tasks: Array.isArray(state.tasks) ? state.tasks : [],
    completions: state.completions && typeof state.completions === "object" ? state.completions : {}
  };
}

function toggleLocalCompletion(state, taskId, date, source) {
  const key = `${taskId}_${date}`;
  if (state.completions[key]) {
    delete state.completions[key];
    return;
  }
  state.completions[key] = {
    taskId,
    date,
    completedAt: new Date().toISOString(),
    source
  };
}
