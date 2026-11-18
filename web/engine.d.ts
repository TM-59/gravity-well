// Types of the WebAssembly modules that `make web` builds from the C sources (the
// EXPORTED_FUNCTIONS lists in the Makefile). Used only by `tsc --checkJs`; nothing is emitted.

/** dist/orbit.js: manual flight and the Hohmann demo (src/flight.c), The Ice Run (src/voyage_web.c). */
interface OrbitModule {
  _flight_reset(mode: number): void;
  _flight_steer(turn: number): void;
  _flight_throttle(burn: number): void;
  _flight_tick(frames: number): void;
  _flight_value(field: number): number;
  _vw_reset(): void;
  _vw_command(command: number): void;
  _vw_tick(frames: number): void;
  _vw_answer(option: number): number;
  _vw_value(field: number): number;
}

/** dist/cr3bp.js: the rotating-frame observatory (src/lagrange.c). */
interface LagrangeModule {
  _lab_set_level(level: number): void;
  _lab_reset(preset: number): void;
  _lab_set_mu(mu: number): void;
  _lab_tick(frames: number): void;
  _lab_value(field: number): number;
  _lab_point_x(index: number): number;
  _lab_point_y(index: number): number;
  _lab_point_c(index: number): number;
  _lab_segment_x1(index: number): number;
  _lab_segment_y1(index: number): number;
  _lab_segment_x2(index: number): number;
  _lab_segment_y2(index: number): number;
  _lab_allowed(x: number, y: number): number;
}

/** Loaded by the classic script dist/orbit.js before web/app.js runs. */
declare function createOrbit(): Promise<OrbitModule>;
/** Loaded by the classic script dist/cr3bp.js before web/lagrange.js runs. */
declare function createLagrange(options?: { wasmBinary?: Uint8Array }): Promise<LagrangeModule>;

/** web/app.js: the C values of the running mode (flight or voyage; the other mode's names are absent). */
interface OrbitState {
  x: number;
  y: number;
  vx: number;
  vy: number;
  time: number;
  heading: number;
  radius: number;
  speed: number;
  energy: number;
  mx: number;
  my: number;
  distance: number;
  relative: number;
  status: number;
  phase: number;
  dv: number;
  mode: number;
  burn1: number;
  burn2: number;
  end: number;
  fuel: number;
  red: number;
  leg: number;
  scene: number;
  banner: number;
  tows: number;
  from: number;
  target: number;
  apo: number;
  peri: number;
  apoRead: number;
  periRead: number;
  matchError: number;
  route: number;
  turned: number;
  count: number;
  wobble: number;
  ending: number;
  loud: number;
  heavy: number;
  command: number;
  a: number;
  e: number;
  periAngle: number;
  radial: number;
  redLine: number;
  held: number;
  holdNeed: number;
  boil: number;
  drift: number;
  cue: number;
}

/** One heavy scene as it opened: whether a banner or a loud readout was still showing. */
interface HeavyEntry {
  id: string;
  banner: number;
  loud: number;
  bannerShown: boolean;
}

/** window.orbitView.snapshot: read-only observation for the smoke test and the playability gate. */
interface OrbitSnapshot extends Omit<OrbitState, 'scene'> {
  paused: boolean;
  voyage: boolean;
  scene: string | null;
  artReady: boolean;
  bannerShown: boolean;
  bannerText: string | null;
  heavyLog: HeavyEntry[];
  sceneText: string | null;
}

/** web/lagrange.js: the C values of the lab (enum LabValue order). */
interface LabState {
  x: number;
  y: number;
  vx: number;
  vy: number;
  time: number;
  c: number;
  c0: number;
  error: number;
  mu: number;
  level: number;
  segments: number;
  status: number;
  preset: number;
  ix: number;
  iy: number;
  growth1: number;
  growth4: number;
  wShort: number;
  wLong: number;
  routh: number;
  hill: number;
}

/** window.lagrangeView.snapshot. */
interface LabSnapshot extends LabState {
  paused: boolean;
  inertial: boolean;
  levelIndex: number;
  points: number[][];
  trailLength: number;
}

interface Window {
  readonly orbitView?: { readonly snapshot: OrbitSnapshot | null };
  readonly lagrangeView?: { readonly snapshot: LabSnapshot | null };
  /** Common test hook of the playability gate (project-context rules/playability.json). */
  __tmGate?: object;
  /** Font sizes passed to fillText, collected by scripts/lagrange-smoke.cjs. */
  canvasFonts?: number[];
}
