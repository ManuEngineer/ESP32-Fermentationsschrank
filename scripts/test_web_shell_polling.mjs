// Local orchestration regression only: no browser, network, packages or board.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(new URL('../lib/fermentation_app/src/web_application_routes.cpp', import.meta.url), 'utf8');
const assetSource = readFileSync(new URL('../lib/fermentation_app/src/web_application_asset.hpp', import.meta.url), 'utf8');
const suffix = ';' + assetSource.match(/R"webjs\(([\s\S]*?)\)webjs"/)[1] + '</script></body></html>';
const script = suffix.slice(1).split('</script>')[0];
const paths = ['status', 'temperatures', 'alerts'];
const actions = ['ack', 'mute', 'stop', 'done', 'start'];
const csrfToken = 'local-fixture';
const drain = async () => { for (let i = 0; i < 40; ++i) await Promise.resolve(); };

function deferred() {
  let resolve, reject;
  const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
  // A response body might never be requested (e.g. non-OK HTTP response).
  promise.catch(() => {});
  return { promise, resolve, reject };
}

class Element {
  constructor() { this.children = []; this.dataset = {}; this.value = ''; }
  checkValidity() { if(this.disabled)return true;const n=Number(this.value);return (!this.required||this.value!=='')&&(this.min===undefined||n>=this.min)&&(this.max===undefined||n<=this.max); }
  addEventListener() {}
  append(...children) { this.children.push(...children); }
  after(...children) { this.afterNodes = children; }
  setAttribute(name, value) { (this.attributes ||= {})[name] = value; }
  replaceChildren(...children) {
    this.children = children;
    this.value = children[0]?.value || '';
  }
}

function snapshot(mode = 'active-run', run = '1') {
  return [
    { homeMode: mode, revisions: { run, programCatalog: '1', userConfiguration:'1' } },
    { temperatures: [] },
    { alerts: mode === 'active-run' ? [{ id: '1', code: 'test-alert', active: true, acknowledged: false, muted: false, severity: 'warning' }] : [] },
  ];
}

async function fixture(language = 'en', storage) {
  const h = { time: 100000, timers: new Map(), requests: [], posts: [], maxGroups: 0, maxRequests: 0, reloads: 0 };
  let timerId = 0;
  const elements = new Map();
  const element = selector => {
    if (!elements.has(selector)) elements.set(selector, new Element());
    return elements.get(selector);
  };
  const buttons = actions.map(a => Object.assign(new Element(), { dataset: { a }, disabled: true }));
  element('#ops').querySelectorAll = () => buttons;
  h.buttons = buttons;
  h.el = element;
  h.connection = element('#connection');
  h.board = element('#board');
  h.rows = () => element('#temps').children.map(e => e.textContent);
  const setTimer = (callback, delay, interval = false) => {
    const id = ++timerId;
    h.timers.set(id, { callback, delay, due: h.time + delay, interval });
    return id;
  };
  const context = vm.createContext({
    document: { querySelector: element, createElement: () => new Element(), documentElement: {} },
    navigator: { language }, window: { __WEB_CSRF__: csrfToken, addEventListener: (event, cb) => { h[event] = cb; } },
    ...(storage ? { localStorage: storage } : {}),
    location: { reload: () => { ++h.reloads; } },
    Option: class extends Element { constructor(name, id) { super(); this.textContent = name; this.value = id; } },
    Date: { now: () => h.time }, AbortController,
    setTimeout: (callback, delay) => setTimer(callback, delay),
    clearTimeout: id => h.timers.delete(id),
    setInterval: (callback, delay) => setTimer(callback, delay, true),
    fetch: (url, options) => {
      if (url === '/api/v1/settings') return Promise.resolve({ok:true,json:async()=>h.settings || {revision:'1',deviceName:'Fixture',timeZone:'Europe/Zurich',nameEditable:true}});
      if (url.startsWith('/api/v1/programs/')) {
        const id=decodeURIComponent(url.split('/').at(-1));
        return Promise.resolve({ok:true,json:async()=>h.detail || {catalogRevision:'1',program:{id,name:'Fixture',builtIn:true,startable:true,targetCelsius:42,durationMinutes:480,preheat:false,sensor:'product-if-available-else-air',defaultSensorMode:'product',completion:{mode:'finish',coolingTargetCelsius:null,holdDurationMinutes:null}}}});
      }
      if (url === '/api/v1/programs') {
        return Promise.resolve({ ok: true, json: async () => ({ catalogRevision: '1', programs: [{ name: 'Fixture', id: 'p1' }] }) });
      }
      if (url === '/internal/ui/run') {
        const response = deferred(), body = deferred();
        h.posts.push({ url, options, started: h.time, reply: (outcome, status = 200) => {
          response.resolve({ status, json: () => body.promise });
          body.resolve({ outcome });
        } });
        return response.promise;
      }
      assert.ok(paths.some(p => url === '/api/v1/' + p), 'only the three polling paths');
      assert.equal(options.cache, 'no-store');
      assert.ok(options.signal instanceof AbortSignal, 'every request shares an abort signal');
      const headers = deferred(), body = deferred();
      const r = {
        url, signal: options.signal, started: h.time, done: false, aborted: false, deferAbort: false,
        header(status = 200) { headers.resolve({ ok: status === 200, status, json: () => body.promise }); },
        body(value) { r.done = true; body.resolve(value); },
        respond(value) { r.header(); r.body(value); },
        fail() { r.done = true; headers.reject(new TypeError('fixture transport failure')); },
        badJson() { r.done = true; body.reject(new SyntaxError('fixture JSON failure')); },
        finishAbort() {
          r.done = true;
          const error = new DOMException('fixture abort', 'AbortError');
          headers.reject(error); body.reject(error);
        },
      };
      r.signal.addEventListener('abort', () => {
        r.aborted = true;
        if (!r.deferAbort) r.finishAbort();
      }, { once: true });
      if (r.signal.aborted) { r.aborted = true; r.finishAbort(); }
      h.requests.push(r);
      const pending = h.requests.filter(x => !x.done);
      h.maxRequests = Math.max(h.maxRequests, pending.length);
      h.maxGroups = Math.max(h.maxGroups, new Set(pending.map(x => x.signal)).size);
      assert.ok(h.maxGroups <= 1, 'no overlapping logical groups, including bodies/abort cleanup');
      assert.ok(h.maxRequests <= 3, 'at most three outstanding polling requests');
      return headers.promise;
    },
  });
  h.ctx = context;
  h.run = code => vm.runInContext(code, context);
  h.state = () => JSON.parse(h.run('JSON.stringify({P,LP,LT,B,Q})'));
  h.group = () => h.requests.slice(-3);
  h.finish = async (data = snapshot()) => {
    h.group().forEach((r, i) => r.respond(data[i])); await drain();
  };
  h.tick = async (milliseconds = 5000) => {
    h.time += milliseconds;
    const timers = [...h.timers.values()].filter(x => x.interval);
    assert.equal(timers.length, 1); assert.equal(timers[0].delay, 5000);
    timers[0].callback(); await drain();
  };
  // Deliver just the deadline; the next regular interval is a separate event.
  h.expire = async () => {
    const deadlines = [...h.timers].filter(([, x]) => !x.interval);
    assert.equal(deadlines.length, 1);
    const [id, timer] = deadlines[0]; assert.equal(timer.delay, 30000);
    h.time = timer.due; h.timers.delete(id); timer.callback(); await drain();
  };
  h.locked = () => assert.ok(buttons.every(b => b.disabled), 'all run actions locked');
  h.noDeadline = () => assert.ok([...h.timers.values()].every(x => x.interval), 'group timer cleaned up');
  h.command = () => h.run("send({t:'ack-message',id:'1'})");
  vm.runInContext("const S='local-fixture',A='password-protected',U=true;let Q=1;", context);
  vm.runInContext(script, context);
  await drain();
  assert.equal(h.requests.length, 3, 'immediate initial group');
  return h;
}

const tests = [];
function test(name, run) { tests.push({ name, run }); }

test('C1 shared locale keys preserve every frozen label and English fallback', async () => {
  // Labels of the previous shell (product ab0ae55 generator), frozen before C1.
  const labels = JSON.parse(readFileSync(new URL('../test/test_web_application_routes/shell_frozen_labels.json', import.meta.url), 'utf8'));
  for (const language of ['de', 'en', 'es', 'fr']) {
    const h = await fixture(language); await h.finish();
    const locale = language === 'fr' ? 'en' : language;
    assert.equal(h.run('lc'), locale);
    for (const [key, value] of Object.entries(labels[locale])) {
      assert.equal(h.run('T[' + JSON.stringify(key) + ']'), value, locale + ':' + key);
    }
  }
});

for (const [language, phase] of [['de', 'Gaerung'], ['en', 'Fermenting'], ['es', 'Fermentando']]) {
  test(`C1 ${language}: full run display, phase minutes and all completion modes`, async () => {
    const h = await fixture(language);
    const data = snapshot();
    const name = '<script>fixture & name</script>';
    data[0].processState = 'fermenting';
    data[0].run = {
      id: 'fixture-run', sourceKind: 'user-program', programName: name, targetCelsius: 42,
      completion: { mode: 'cool-hold-duration', coolingTargetCelsius: 5, holdDurationMinutes: 90 },
      time: { scope: 'phase', elapsedSeconds: 90061, remainingSeconds: 61, remainingKind: 'timed' },
    };
    await h.finish(data);
    const rows = h.rows();
    assert.ok(rows.some(x => x.endsWith(name)), 'name is a literal text value');
    assert.ok(rows.some(x => x.endsWith(phase)));
    assert.ok(rows.some(x => x.endsWith('42.0 °C')));
    assert.ok(rows.some(x => x.endsWith('25:01')), 'elapsed hours do not wrap at 24');
    assert.ok(rows.some(x => x.endsWith('0:02')), 'positive remaining minutes round up');
    assert.ok(rows.some(x => x.includes('5.0 °C') && x.endsWith('1:30')));
    for (const mode of ['finish', 'cool-finish', 'cool-hold-duration', 'cool-hold-manual']) {
      await h.tick(); data[0].run.completion.mode = mode; await h.finish(data);
      assert.ok(h.rows().some(x => x.includes(h.run('T[' + JSON.stringify('c-' + mode) + ']'))));
    }
    for (const token of ['boot', 'safe-boot', 'standby', 'preheating', 'waiting-for-product', 'reaching-target', 'qualifying-target', 'fermenting', 'cooling', 'cool-holding', 'manual-holding', 'completed', 'recovery-evaluation', 'fault', 'service-mode']) {
      assert.equal(typeof h.run('T[' + JSON.stringify('p-' + token) + ']'), 'string');
    }
    const last = h.rows();
    for (let i = 0; i < 4; ++i) await h.tick();
    assert.deepEqual(h.rows(), last, 'LP values never count on while waiting');
    assert.equal(h.board.className, 'x'); h.locked();
  });

  test(`C1 ${language}: missing, unknown, unbounded and cleared details`, async () => {
    const h = await fixture(language), data = snapshot();
    data[0].processState = 'future-phase';
    data[0].run = {
      sourceKind: 'manual-timed', programName: null, targetCelsius: null,
      completion: null,
      time: { scope: 'phase', elapsedSeconds: null, remainingSeconds: null, remainingKind: 'unknown' },
    };
    await h.finish(data);
    const unavailable = h.run('T.unavailable');
    assert.ok(h.rows().some(x => x.endsWith('future-phase')));
    assert.equal(h.rows().slice(4, 10).filter(x => x.endsWith(unavailable)).length, 4, 'run detail rows only');
    assert.ok(h.rows().some(x => x.endsWith(h.run("T['manual-timed']"))));
    assert.equal(h.buttons.find(b => b.dataset.a === 'stop').disabled, false, 'display fields never create a new command gate');
    for (const kind of ['unbounded', 'not-applicable', 'future-kind']) {
      await h.tick(); data[0].run.time.remainingKind = kind; await h.finish(data);
      assert.ok(h.rows().some(x => x.endsWith(kind === 'future-kind' ? kind : h.run('T[' + JSON.stringify(kind) + ']'))));
    }
    await h.tick(); await h.finish(snapshot('standby', '2'));
    assert.equal(h.rows().length, 6, 'old run details disappear with the new snapshot (name, 3 temperatures, network, clock)');
  });
}

const deviceLabels = {
  de: { network: 'Netzwerk', clock: 'Uhrzeit', ap: 'Eigener AP', wifi: 'Heimnetz', up: 'verbunden', down: 'getrennt', untrusted: 'nicht synchronisiert', unavailable: 'Nicht verfuegbar' },
  en: { network: 'Network', clock: 'Time', ap: 'Own AP', wifi: 'Home Wi-Fi', up: 'connected', down: 'disconnected', untrusted: 'not synchronized', unavailable: 'Unavailable' },
  es: { network: 'Red', clock: 'Hora', ap: 'AP propio', wifi: 'Wi-Fi domestico', up: 'conectado', down: 'desconectado', untrusted: 'no sincronizada', unavailable: 'No disponible' },
};
const infoModes = ['standby', 'active-run', 'waiting', 'completed', 'restricted', 'recovery', 'unavailable'];

for (const language of ['de', 'en', 'es']) {
  const l = deviceLabels[language];
  test(`PR207 ${language}: device name, separate network mode/state and local clock`, async () => {
    const h = await fixture(language), data = snapshot('standby', '1');
    Object.assign(data[0], { deviceName: '<b>Schrank & Co</b>', networkMode: 'ap-only', networkState: 'connected', clock: { local: '2026-10-20 14:03', zone: 'Europe/Zurich' } });
    await h.finish(data);
    let rows = h.rows();
    assert.equal(rows[0], '<b>Schrank & Co</b>', 'device name is literal text');
    assert.ok(rows.includes(`${l.network}: ${l.ap} · ${l.up}`), rows.join('|'));
    assert.ok(rows.includes(`${l.clock}: 2026-10-20 14:03 Europe/Zurich`));
    // The network words never reuse the browser poll status words.
    const online = h.run('T.online'), offline = h.run('T.offline');
    for (const row of rows.filter(x => x.startsWith(l.network))) {
      assert.ok(!row.includes(online) && !row.includes(offline), row);
    }
    await h.tick(); data[0].networkMode = 'home-wifi'; data[0].networkState = 'disconnected'; await h.finish(data);
    assert.ok(h.rows().includes(`${l.network}: ${l.wifi} · ${l.down}`));
    // Mode and state are independent values.
    await h.tick(); data[0].networkMode = 'ap-only'; data[0].networkState = 'disconnected'; await h.finish(data);
    assert.ok(h.rows().includes(`${l.network}: ${l.ap} · ${l.down}`));
  });

  test(`PR207 ${language}: unknown, missing and untrusted values are never shown as valid`, async () => {
    const h = await fixture(language), data = snapshot('standby', '1');
    // Fields absent (older/partial response): everything unavailable.
    await h.finish(data);
    let rows = h.rows();
    assert.equal(rows[0], l.unavailable);
    assert.ok(rows.includes(`${l.network}: ${l.unavailable} · ${l.unavailable}`));
    assert.ok(rows.includes(`${l.clock}: ${l.untrusted}`));
    for (const [mode, state] of [['selection-required', 'unavailable'], ['future-mode', 'future-state']]) {
      await h.tick(); Object.assign(data[0], { deviceName: null, networkMode: mode, networkState: state, clock: null }); await h.finish(data);
      rows = h.rows();
      assert.equal(rows[0], l.unavailable);
      assert.ok(rows.includes(`${l.network}: ${l.unavailable} · ${l.unavailable}`), rows.join('|'));
      assert.ok(rows.includes(`${l.clock}: ${l.untrusted}`));
    }
  });
}

test('PR207 all home modes show the read-only info without changing any action gate', async () => {
  for (const mode of infoModes) {
    const plain = await fixture('en'); await plain.finish(snapshot(mode, '1'));
    const gates = plain.buttons.map(b => b.disabled);
    const h = await fixture('en'), data = snapshot(mode, '1');
    Object.assign(data[0], { deviceName: 'Fermenter', networkMode: 'home-wifi', networkState: 'connected', clock: { local: '2026-10-20 14:03', zone: 'Europe/Zurich' } });
    await h.finish(data);
    assert.ok(h.rows().includes('Fermenter'), mode);
    assert.ok(h.rows().includes('Network: Home Wi-Fi · connected'), mode);
    assert.deepEqual(h.buttons.map(b => b.disabled), gates, 'display fields never open or close an action: ' + mode);
  }
});

test('PR207 info rows come only from the fetched status and never advance on their own', async () => {
  const h = await fixture('en'), data = snapshot('standby', '1');
  Object.assign(data[0], { deviceName: 'Fermenter', networkMode: 'ap-only', networkState: 'connected', clock: { local: '2026-10-20 14:03', zone: 'Europe/Zurich' } });
  await h.finish(data);
  const before = h.rows();
  for (let i = 0; i < 4; ++i) await h.tick();
  assert.deepEqual(h.rows(), before, 'the browser clock is never used as device time');
});

test('real send(): canonical sequence and CSRF headers across successive mutations', async () => {
  const h = await fixture(); await h.finish();
  for (const sequence of ['1', '2']) {
    assert.ok(h.state().P, 'valid action basis before the actual send()');
    const command = h.command();
    const post = h.posts.at(-1);
    assert.equal(post.url, '/internal/ui/run');
    assert.equal(post.options.method, 'POST');
    assert.equal(post.options.headers['X-UI-Mutation-Seq'], sequence);
    assert.equal(post.options.headers['X-CSRF-Token'], csrfToken);
    assert.equal(post.options.headers['Content-Type'], 'application/json');
    assert.equal(Object.hasOwn(post.options.headers, 'X-Mutation-Seq'), false);
    post.reply('applied'); await drain();
    await h.finish(); await command;
  }
  assert.equal(h.posts.length, 2); assert.equal(h.state().Q, 3);
  assert.equal(h.reloads, 0); h.noDeadline();
});

test('initial fetch, normal five-second ticks and fresh Online', async () => {
  const h = await fixture();
  assert.equal(h.connection.dataset.state, 'offline');
  await h.finish();
  assert.equal(h.connection.dataset.state, 'online');
  assert.ok(h.buttons.find(b => b.dataset.a === 'stop').disabled === false);
  h.noDeadline(); await h.tick(); assert.equal(h.requests.length, 6);
  await h.finish(); h.noDeadline();
});

test('headers do not release the lock; skipped ticks keep stale age current', async () => {
  const h = await fixture(); await h.finish(); await h.tick();
  h.group().forEach(r => r.header());
  await h.tick(); await h.tick(); await h.tick();
  assert.equal(h.requests.length, 6);
  assert.equal(h.connection.dataset.state, 'stale');
  assert.match(h.connection.textContent, /20 s$/); h.locked();
  h.group().forEach((r, i) => r.body(snapshot()[i])); await drain();
  h.noDeadline();
});

test('24-second success is not timed out, but remains stale; next group recovers', async () => {
  const h = await fixture();
  await h.tick(); await h.tick(); await h.tick(); await h.tick();
  assert.equal(h.requests.length, 3); assert.equal(h.connection.dataset.state, 'offline');
  h.time += 4000; await h.finish();
  assert.equal(h.group().some(r => r.aborted), false);
  assert.equal(h.connection.dataset.state, 'stale'); h.locked();
  await h.tick(); await h.finish(); assert.equal(h.connection.dataset.state, 'online');
});

for (const phase of ['headers', 'body']) {
  test(`30-second ${phase} deadline aborts all siblings; no immediate retry or backlog`, async () => {
    const h = await fixture();
    if (phase === 'body') { h.group().forEach(r => r.header()); await drain(); }
    for (let i = 0; i < 5; ++i) await h.tick();
    h.time += 4999;
    assert.equal(h.requests.length, 3); assert.ok(h.group().every(r => !r.aborted));
    await h.expire(); assert.ok(h.group().every(r => r.aborted && r.done));
    h.locked(); h.noDeadline(); assert.equal(h.requests.length, 3);
    await h.tick(); assert.equal(h.requests.length, 6); await h.finish();
    assert.equal(h.connection.dataset.state, 'online');
  });
}

for (const failure of ['transport', 'http', 'json']) {
  test(`early ${failure} failure aborts siblings and waits for delayed abort settlement`, async () => {
    const h = await fixture(); await h.finish(); await h.tick();
    const group = h.group(); group[2].deferAbort = true;
    if (failure === 'transport') group[0].fail();
    if (failure === 'http') group[0].header(503);
    if (failure === 'json') { group[0].header(); await drain(); group[0].badJson(); }
    await drain();
    assert.ok(group.every(r => r.aborted)); h.locked();
    assert.equal(h.connection.dataset.state, 'stale');
    await h.tick(); assert.equal(h.requests.length, 6);
    group[2].finishAbort(); await drain(); h.noDeadline();
    assert.equal(h.requests.length, 6); await h.tick();
    assert.equal(h.requests.length, 9); await h.finish();
    assert.equal(h.connection.dataset.state, 'online');
  });
}

const outcomes = ['applied', 'stale', 'busy', 'confirmation-required', 'rejected', 'too-large', 'write-failed'];
for (const outcome of outcomes) {
  for (const refresh of ['fresh', 'aborted', 'failed', 'old']) {
    test(`real send(): ${outcome}, refresh ${refresh}, no old action basis`, async () => {
      const h = await fixture(); await h.finish(); await h.tick();
      const old = h.group(); old.forEach(r => { r.header(); r.deferAbort = true; });
      await drain(); assert.ok(h.state().P, 'valid pre-mutation basis');
      let completed = false;
      const command = h.command(); command.then(() => { completed = true; });
      h.time += 1; const mutationFinished = h.time; h.posts[0].reply(outcome); await drain();
      assert.equal(h.state().P, null); assert.ok(h.state().LP);
      assert.equal(Boolean(h.state().B), false, 'P was invalidated before B was released');
      h.locked(); assert.equal(h.board.className, 'x');
      assert.ok(old.every(r => r.aborted)); assert.equal(completed, false);
      await h.tick(); h.locked(); assert.equal(h.requests.length, 6);
      // Late old success despite an already signalled abort must be discarded.
      old.forEach((r, i) => r.body(snapshot('completed', 'old')[i])); await drain();
      assert.equal(h.requests.length, 9); assert.equal(completed, false);
      assert.ok(h.group().every(r => r.started > mutationFinished));
      assert.equal(h.state().P, null); assert.equal(h.state().LP[0].revisions.run, '1'); h.locked();
      await h.tick(); h.locked(); assert.equal(h.requests.length, 9);
      if (refresh === 'aborted') await h.expire();
      else if (refresh === 'failed') { h.group()[1].header(503); await drain(); }
      else {
        if (refresh === 'old') h.time += 19000;
        await h.finish(snapshot('standby', '2'));
      }
      await command; assert.equal(completed, true); assert.equal(h.state().Q, 2);
      if (refresh === 'fresh') {
        assert.ok(h.state().P); assert.equal(h.connection.dataset.state, 'online');
        assert.equal(h.buttons.find(b => b.dataset.a === 'start').disabled, false);
        assert.equal(h.buttons.find(b => b.dataset.a === 'stop').disabled, true);
      } else {
        assert.equal(h.state().P, null); h.locked(); assert.equal(h.board.className, 'x');
        await h.tick(); await h.finish(snapshot('standby', '3'));
        assert.equal(h.buttons.find(b => b.dataset.a === 'start').disabled, false);
      }
      h.noDeadline(); assert.equal(h.reloads, 0);
    });
  }
}

test('mutation at group completion and one coalesced refresh reservation', async () => {
  const h = await fixture(); await h.finish(); await h.tick();
  const command = h.command();
  // Resolve old bodies and the mutation in the same host turn.
  h.group().forEach((r, i) => r.respond(snapshot()[i]));
  h.posts[0].reply('applied'); await drain(); h.locked();
  assert.equal(h.requests.length, 9);
  await h.finish(snapshot('standby', '2')); await command;
  await h.tick(); const old = h.group(); old.forEach(r => { r.deferAbort = true; });
  // Exercise coalescing of explicit requests while old cleanup is pending.
  h.run('P=null;ui()'); const a = h.run('rf(1)'), b = h.run('rf(1)');
  assert.equal(a, b, 'only one pending explicit refresh promise');
  await h.tick(); assert.equal(h.requests.length, 12);
  old.forEach(r => r.finishAbort()); await drain();
  assert.equal(h.requests.length, 15); await h.finish(); await Promise.all([a, b]);
  assert.equal(h.requests.length, 15); h.noDeadline();
});

for (const phase of ['no-active-group', 'pending-headers']) {
  test(`real send(): ${phase}, fresh snapshot even within the same clock millisecond`, async () => {
    const h = await fixture(); await h.finish();
    if (phase === 'pending-headers') await h.tick();
    const command = h.command(); h.posts[0].reply('applied'); await drain();
    h.locked(); assert.equal(h.state().P, null);
    assert.equal(h.requests.length, phase === 'pending-headers' ? 9 : 6);
    await h.finish(snapshot('standby', 'new')); await command;
    assert.equal(h.state().P[0].revisions.run, 'new');
    assert.equal(h.state().LP[0].revisions.run, 'new');
    assert.equal(h.buttons.find(b => b.dataset.a === 'start').disabled, false);
  });
}

test('an unacknowledged abort keeps the lock across deadline and further ticks', async () => {
  const h = await fixture(); await h.finish(); await h.tick();
  assert.ok(h.state().P);
  h.group().forEach(r => { r.deferAbort = true; });
  // No interval delivery before the deadline, as with browser throttling.
  await h.expire(); await h.tick(); await h.tick();
  assert.equal(h.requests.length, 6); h.locked();
  assert.equal(h.state().P, null); assert.equal(h.connection.dataset.state, 'stale');
  h.group().forEach(r => r.finishAbort()); await drain(); h.noDeadline();
  await h.tick(); assert.equal(h.requests.length, 9); await h.finish();
});

// ---- Plan a671c33: browser-individual language selection (PR 208) ----
const staticTexts = {
  de: { login: 'Anmelden', pl: 'Passwort', logout: 'Abmelden', pt: 'Passwortschutz aktiv', stop: 'Stop', sure: 'Sicher?', online: 'Online' },
  en: { login: 'Login', pl: 'Password', logout: 'Logout', pt: 'Password protection enabled', stop: 'Stop', sure: 'Sure?', online: 'Online' },
  es: { login: 'Iniciar sesion', pl: 'Contrasena', logout: 'Cerrar sesion', pt: 'Proteccion por contrasena activa', stop: 'Parar', sure: 'Seguro?', online: 'En linea' },
};
const selectLanguage = (h, value) => h.run(`(()=>{sw.value=${JSON.stringify(value)};sw.onchange();})()`);
const memory = (initial) => { const store = {}; if (initial !== undefined) store.lang = initial; return store; };
const throwingStorage = { get lang() { throw new DOMException('blocked', 'SecurityError'); }, set lang(value) { throw new DOMException('quota', 'QuotaExceededError'); } };
const writeThrowsStorage = { get lang() { return undefined; }, set lang(value) { throw new DOMException('quota', 'QuotaExceededError'); } };

test('language: browser suggestion on first visit, English fallback, stored valid code wins', async () => {
  for (const [navigatorLanguage, expected] of [['de-CH', 'de'], ['en-US', 'en'], ['es-ES', 'es'], ['fr-FR', 'en'], ['', 'en'], ['constructor', 'en']]) {
    const h = await fixture(navigatorLanguage, memory());
    assert.equal(h.run('lc'), expected, navigatorLanguage);
    assert.equal(h.run('document.documentElement.lang'), expected);
    assert.equal(h.el('#lang').value, expected, 'the selector shows the active language');
  }
  for (const stored of ['de', 'en', 'es']) {
    const h = await fixture('fr-FR', memory(stored));
    assert.equal(h.run('lc'), stored, 'stored choice has priority over the suggestion');
  }
});

test('language: invalid or tampered stored values are ignored and never reused', async () => {
  for (const stored of ['xx', 'DE', 'de ', 'constructor', '__proto__', '', 'deutsch', 'de,en', null, 7]) {
    const h = await fixture('es-ES', memory(stored));
    assert.equal(h.run('lc'), 'es', 'falls back to the suggestion: ' + String(stored));
    assert.deepEqual(Object.keys(staticTexts.es).every(k => h.run(`typeof T[${JSON.stringify(k)}]`) === 'string'), true);
  }
  const h = await fixture('fr-FR', memory('xx'));
  assert.equal(h.run('lc'), 'en');
});

test('language: switching re-renders static, dynamic and action texts in each language', async () => {
  const h = await fixture('en', memory()); const data = snapshot('active-run', '1');
  await h.finish(data);
  for (const language of ['de', 'es', 'en', 'de']) {
    const requests = h.requests.length;
    selectLanguage(h, language);
    const t = staticTexts[language];
    assert.equal(h.run('lc'), language);
    assert.equal(h.run('document.documentElement.lang'), language);
    assert.equal(h.el('#login button').textContent, t.login);
    assert.equal(h.el('#password-label').textContent, t.pl);
    assert.equal(h.el('#logout').textContent, t.logout);
    assert.equal(h.el('#auth-state').textContent, t.pt);
    assert.equal(h.buttons.find(b => b.dataset.a === 'stop').textContent, t.stop);
    assert.equal(h.connection.textContent, t.online, 'dashboard connection text');
    assert.ok(h.rows().some(x => x.startsWith(h.run("T['cabinet-air']"))), 'temperature rows');
    assert.equal(h.requests.length, requests, 'a language change sends no request');
    assert.equal(h.posts.length, 0);
  }
});

test('language: choice survives a reload and stays independent between browsers', async () => {
  const first = memory(); const other = memory();
  const a = await fixture('en', first); selectLanguage(a, 'de');
  assert.equal(first.lang, 'de');
  const reloaded = await fixture('en', first);          // same browser storage
  assert.equal(reloaded.run('lc'), 'de');
  const partner = await fixture('en', other); selectLanguage(partner, 'es');
  assert.equal(other.lang, 'es'); assert.equal(first.lang, 'de');
  assert.equal((await fixture('en', first)).run('lc'), 'de');
  assert.equal((await fixture('en', other)).run('lc'), 'es');
});

test('language: invalid selector values are rejected and only validated codes are stored', async () => {
  const store = memory(); const h = await fixture('de-DE', store);
  for (const bad of ['zz', 'constructor', '', 'EN']) {
    selectLanguage(h, bad);
    assert.equal(h.run('lc'), 'en', 'falls back to English: ' + bad);
    assert.ok(['en'].includes(store.lang) && /^(de|en|es)$/.test(store.lang));
  }
  selectLanguage(h, 'es'); assert.equal(store.lang, 'es');
});

test('language: missing, blocked or write-throwing storage never throws and keeps the page language', async () => {
  for (const [name, storage] of [['no storage', undefined], ['blocked', throwingStorage], ['write throws', writeThrowsStorage]]) {
    const h = await fixture('en-US', storage);
    assert.equal(h.run('lc'), 'en', name);
    selectLanguage(h, 'de');
    assert.equal(h.run('lc'), 'de', name + ': switched');
    assert.equal(h.el('#login button').textContent, 'Anmelden', name);
    selectLanguage(h, 'es');
    assert.equal(h.run('lc'), 'es', name + ': stays effective for the page');
  }
});

test('language: an active confirmation is re-rendered in the new language without changing K/KC/gates', async () => {
  const h = await fixture('en', memory()); await h.finish();
  const stop = h.buttons.find(b => b.dataset.a === 'stop');
  h.ctx.__t = stop; h.run('ops.onclick({target:__t})');
  assert.equal(h.run('K'), 'stop'); assert.equal(stop.textContent, 'Sure?');
  const kc = h.run('KC'), gates = h.buttons.map(b => b.disabled), posts = h.posts.length;
  for (const language of ['es', 'de', 'en']) {
    selectLanguage(h, language);
    assert.equal(stop.textContent, staticTexts[language].sure);
    assert.equal(h.run('K'), 'stop'); assert.equal(h.run('KC'), kc);
    assert.deepEqual(h.buttons.map(b => b.disabled), gates);
    assert.equal(h.posts.length, posts);
  }
  h.run('ops.onclick({target:__t})');                   // second click still confirms once
  assert.equal(h.run('K'), ''); await drain(); assert.equal(h.posts.length, posts + 1);
});

test('language: no home mode changes P or any action gate', async () => {
  for (const mode of infoModes) {
    const plain = await fixture('en', memory()); await plain.finish(snapshot(mode, '1'));
    const h = await fixture('en', memory()); await h.finish(snapshot(mode, '1'));
    const before = JSON.stringify([h.buttons.map(b => b.disabled), h.state().P === null]);
    selectLanguage(h, 'de'); selectLanguage(h, 'es');
    assert.equal(JSON.stringify([h.buttons.map(b => b.disabled), h.state().P === null]), before, mode);
    assert.deepEqual(h.buttons.map(b => b.disabled), plain.buttons.map(b => b.disabled), mode);
  }
});

// Setup (provisioning) shell: its own small script, same selection contract.
const provisioningLiteral = source.match(/static constexpr char kSuffix\[\] =([\s\S]*?);\n\s+staticBodySuffix = kSuffix;/);
assert.ok(provisioningLiteral, 'extract the actual provisioning suffix');
const provisioningScript = [...provisioningLiteral[1].matchAll(/"(?:[^"\\]|\\.)*"/g)].map(x => JSON.parse(x[0])).join('').split('</script>')[0];
const provisioningIds = ['a', 'b', 'c', 'e', 'x', 'y', 'g'];

function provisioning(language, storage, options = {}) {
  const elements = new Map();
  const id = name => { if (!elements.has(name)) elements.set(name, { textContent: '', hidden: false, value: '', checked: false }); return elements.get(name); };
  const fetches = [];
  const context = vm.createContext({
    document: {
      getElementById: id, documentElement: {},
      querySelector: () => ({ value: options.mode || 'p' }), querySelectorAll: () => [],
    },
    navigator: { language },
    fetch: (...args) => { fetches.push(args); if (!options.fetch) throw new Error('no request'); return options.fetch(...args); },
    ...(storage ? { localStorage: storage } : {}),
  });
  vm.runInContext(provisioningScript, context);
  return { id, fetches, run: code => vm.runInContext(code, context), context };
}

test('language (setup shell): suggestion, stored choice, validation and independent texts', async () => {
  for (const [navigatorLanguage, expected] of [['de-CH', 'de'], ['es-AR', 'es'], ['en-GB', 'en'], ['fr-FR', 'en'], ['constructor', 'en']]) {
    const p = provisioning(navigatorLanguage, memory());
    assert.equal(p.context.document.documentElement.lang, expected, navigatorLanguage);
    for (const key of provisioningIds) assert.equal(p.id(key).textContent, p.run(`L.${expected}.${key}`), expected + ':' + key);
  }
  for (const stored of ['xx', 'constructor', 'DE', '']) {
    assert.equal(provisioning('es-ES', memory(stored)).context.document.documentElement.lang, 'es', String(stored));
  }
  assert.equal(provisioning('fr-FR', memory('de')).context.document.documentElement.lang, 'de');
});

test('language (setup shell): switching sets every text, stores only validated codes and tolerates blocked storage', async () => {
  const store = memory(); const p = provisioning('en-US', store);
  for (const language of ['de', 'es', 'en']) {
    p.id('lang').value = language; p.id('lang').onchange();
    assert.equal(p.context.document.documentElement.lang, language);
    assert.equal(store.lang, language);
    for (const key of provisioningIds) assert.equal(p.id(key).textContent, p.run(`L.${language}.${key}`), language + ':' + key);
  }
  p.id('lang').value = 'zz'; p.id('lang').onchange();
  assert.equal(p.context.document.documentElement.lang, 'en'); assert.equal(store.lang, 'en');
  for (const storage of [undefined, throwingStorage, writeThrowsStorage]) {
    const q = provisioning('en-US', storage);
    q.id('lang').value = 'de'; q.id('lang').onchange();
    assert.equal(q.context.document.documentElement.lang, 'de');
    assert.equal(q.id('c').textContent, q.run('L.de.c'));
  }
  // Dashboard and setup shell share the key: the choice made in setup carries over.
  const shared = memory(); provisioning('en-US', shared).id('lang').onchange;
  const setup = provisioning('en-US', shared); setup.id('lang').value = 'es'; setup.id('lang').onchange();
  assert.equal((await fixture('en-US', shared)).run('lc'), 'es');
});


// Setup shell: the visible result message keeps its kind across language changes.
const setupOutcomes = [
  ['ok', { ok: true, status: 200, json: async () => ({}) }],
  ['na', { ok: false, status: 403, json: async () => ({ error: 'provisioning-not-allowed' }) }],
  ['ap', { ok: false, status: 409, json: async () => ({}) }],
  ['ic', { ok: false, status: 422, json: async () => ({}) }],
  ['rc', { ok: false, status: 503, json: async () => ({ error: 'recovery-required' }) }],
  ['fl', { ok: false, status: 500, json: async () => ({}) }],
];
const submitSetup = async p => { await p.id('f').onsubmit({ preventDefault() {} }); await drain(); };
const switchSetup = (p, language) => { p.id('lang').value = language; p.id('lang').onchange(); };
const formState = p => JSON.stringify(['lw', 'x', 'lk', 'w', 'n', 'k'].map(k => [p.id(k).hidden, p.id(k).value, p.id(k).checked]));

for (const [key, response] of setupOutcomes) {
  test(`language (setup shell): visible message "${key}" is re-rendered DE -> ES -> EN, form state kept, no request`, async () => {
    const p = provisioning('de-DE', memory(), { fetch: async () => response });
    assert.equal(p.id('m').textContent, '', 'no message before an action');
    switchSetup(p, 'es'); assert.equal(p.id('m').textContent, '', 'without a message the element stays empty');
    switchSetup(p, 'de');
    p.id('w').value = 'pw'; p.id('n').value = '1234';
    await submitSetup(p);
    assert.equal(p.fetches.length, 1);
    assert.equal(p.id('m').textContent, p.run(`L.de.${key}`));
    const form = formState(p);
    for (const language of ['es', 'en', 'de', 'es', 'en']) {
      switchSetup(p, language);
      assert.equal(p.id('m').textContent, p.run(`L.${language}.${key}`), `${key} in ${language}`);
      assert.equal(formState(p), form, 'form state is untouched by the language');
    }
    assert.equal(p.fetches.length, 1, 'the language change sends no request');
  });
}

test('language (setup shell): the unconfirmed-disable hint and a transport error follow the language', async () => {
  const p = provisioning('de-DE', memory(), { mode: 'd' });
  await submitSetup(p);                                   // confirmation checkbox not ticked
  assert.equal(p.id('m').textContent, p.run('L.de.y'));
  switchSetup(p, 'es'); assert.equal(p.id('m').textContent, p.run('L.es.y'));
  assert.equal(p.fetches.length, 0);
  const t = provisioning('en-GB', memory(), { fetch: async () => { throw new TypeError('offline'); } });
  await submitSetup(t); assert.equal(t.id('m').textContent, t.run('L.en.fl'));
  switchSetup(t, 'de'); assert.equal(t.id('m').textContent, t.run('L.de.fl'));
});

test('language (setup shell): a pending request answers in the language selected at response time', async () => {
  let release; const gate = new Promise(resolve => { release = resolve; });
  const p = provisioning('de-DE', memory(), { fetch: () => gate });
  const pending = p.id('f').onsubmit({ preventDefault() {} });
  await drain(); assert.equal(p.id('m').textContent, '', 'no message while pending');
  switchSetup(p, 'es'); assert.equal(p.id('m').textContent, '');
  release({ ok: false, status: 409, json: async () => ({}) });
  await pending; await drain();
  assert.equal(p.id('m').textContent, p.run('L.es.ap'), 'answered in the language chosen meanwhile');
  assert.equal(p.fetches.length, 1);
  switchSetup(p, 'en'); assert.equal(p.id('m').textContent, p.run('L.en.ap'));
});

for (const language of ['de','en','es']) test('C1 navigation/alerts '+language, async () => {
  const h=await fixture(language); const data=snapshot();
  data[2].alerts=Array.from({length:16},(_,i)=>({id:String(i+1),code:'code-'+i,severity:i?'information':'safety-fault',active:true,acknowledged:i===1,muted:i===2,resolved:i===3,decisionRequired:i===4}));
  await h.finish(data);
  assert.equal(h.run('nv.hidden'),false);
  const requests=h.requests.length;
  h.run("nb[1].onclick()");
  assert.equal(h.requests.length,requests);
  assert.equal(h.run('H'),'alerts');
  assert.equal(h.run('nb[1].attributes["aria-current"]'),'page');
  assert.equal(h.run('ls.children.length'),17);
  assert.equal(h.run('ls.children[1].textContent').split(' · ')[0],'code-0');
  assert.equal(h.run('ls.children[16].textContent').split(' · ')[0],'code-15');
  assert.equal(h.run('ls.children[2].children[0].disabled'),true);
  assert.equal(h.run('ls.children[3].children[1].disabled'),true);
  assert.equal(h.run('ls.children[4].children[0].disabled'),true);
  h.run("location.hash='#invalid';fr()");assert.equal(h.run('H'),'alerts');
  h.run("location.hash='#overview';fr()");assert.equal(h.run('H'),'overview');
  h.run("location.hash='#alerts';fr();P=null;ui()");
  assert.equal(h.run('ls.children[1].children[0].disabled'),true);
  h.run('P=LP;B=true;ui()');assert.equal(h.run('ls.children[1].children[0].disabled'),true);
  h.run('B=false;ui();ls.children[1].children[0].onclick()');
  assert.equal(JSON.parse(h.posts[0].options.body).i.id,'1');
  assert.equal(JSON.parse(h.posts[0].options.body).i.t,'ack-message');
  assert.equal(h.run('ls.children[1].children[0].disabled'),true);
});

test('C2 program details, overrides and confirmation binding', async()=>{
  const h=await fixture();await h.finish(snapshot('standby'));
  assert.equal(h.run('PD.id'),'p1');
  h.run("nb[2].onclick()");assert.equal(h.run('pd.hidden'),false);
  assert.deepEqual(JSON.parse(h.run('JSON.stringify(sc())')),{p:'p1'});
  h.run("pf.x.value='43';pf.x.onchange();ps.onclick()");
  assert.equal(h.posts.length,0);assert.equal(h.run('K'),'start');
  h.run("pf.d.value='90';pf.d.onchange()");assert.equal(h.run('K'),'');
  h.run('ps.onclick();ps.onclick()');assert.equal(h.posts.length,1);
  assert.deepEqual(JSON.parse(h.posts[0].options.body).i,{t:'start-program',c:{p:'p1',x:43,d:90}});
});
test('C2 null/not-startable and stale program details cannot start',async()=>{
  const h=await fixture();h.detail={catalogRevision:'1',program:{id:'p1',name:'TBD',startable:false,targetCelsius:null,durationMinutes:null,preheat:false,defaultSensorMode:'air',completion:{mode:'finish',coolingTargetCelsius:null,holdDurationMinutes:null}}};
  await h.finish(snapshot('standby'));assert.equal(h.run('ps.disabled'),true);
  assert.equal(h.run('pf.x.value'),'');assert.equal(h.run('pf.d.value'),'');
  h.detail.catalogRevision='2';await h.run('ld()');
  assert.equal(h.run('PD'),null);assert.equal(h.run('pg.value'),'');assert.equal(h.run('PC'),'');
  assert.equal(h.posts.length,0);
});

for(const language of ['de','en','es'])test('C3 settings confirmation and read-only zone '+language,async()=>{
  const h=await fixture(language);await h.finish(snapshot('standby'));
  h.run('nb[3].onclick()');await drain();
  assert.equal(h.run('SD.timeZone'),'Europe/Zurich');assert.equal(h.run('sz.textContent'),'Europe/Zurich');
  assert.equal(h.run('sn.attributes["aria-label"]'),h.run('T.name'));
  h.run("sn.value='New name';sn.oninput();sb.onclick()");assert.equal(h.posts.length,0);
  h.run("sn.value='Other';sn.oninput();sb.onclick()");assert.equal(h.posts.length,0);
  h.run('sb.onclick()');assert.equal(h.posts.length,1);
  assert.deepEqual(JSON.parse(h.posts[0].options.body),{v:1,r:{u:'1'},i:{t:'set-device-name',n:'Other'}});
  assert.equal(h.run('sb.disabled'),true);
});
test('C3 settings nameEditable false and stale lock writes',async()=>{
 const h=await fixture();await h.finish();h.settings={revision:'1',deviceName:'Fixed',timeZone:'Europe/Zurich',nameEditable:false};
 h.run('nb[3].onclick()');await drain();assert.equal(h.run('sn.disabled'),true);h.run('sb.onclick()');assert.equal(h.posts.length,0);
 h.run('SD.nameEditable=true;P=null;ui()');assert.equal(h.run('sb.disabled'),true);assert.equal(h.run('sn.disabled'),true);
});

for(const language of ['de','en','es'])test('C5 uncertain settings outcome locks sequence and refreshes '+language,async()=>{
 const h=await fixture(language);await h.finish(snapshot('standby'));h.run('nb[3].onclick()');await drain();
 const command=h.run("send({t:'set-device-name',n:'Maybe'})");
 h.posts[0].reply('unavailable',503);await drain();
 assert.equal(h.run('Q'),0);assert.ok(h.run('nt.textContent').includes(h.run('T.uncertain')));assert.equal(h.run('sb.disabled'),true);
 await h.finish(snapshot('standby'));await command;
 assert.ok(h.run('SD'));assert.equal(h.run('sb.disabled'),true);assert.equal(h.reloads,0);
});
test('C5 rendered input constraints block invalid overrides',async()=>{
 const h=await fixture();await h.finish(snapshot('standby'));
 h.run("pf.x.value='';pf.x.onchange()");assert.equal(h.run('ps.disabled'),true);
 h.run("pf.x.value='46';pf.x.onchange()");assert.equal(h.run('ps.disabled'),true);
 h.run("pf.x.value='43';pf.c.value='cool-hold-duration';pf.c.onchange()");assert.equal(h.run('ps.disabled'),true);
 h.run("pf.k.value='5';pf.l.value='90';pf.l.onchange()");assert.equal(h.run('ps.disabled'),false);
 h.run('ps.onclick();ps.onclick()');const intent=JSON.parse(h.posts[0].options.body).i;
 assert.equal(intent.c.c,'cool-and-hold-for-duration');assert.equal(intent.c.k,5);assert.equal(intent.c.l,90);
});


for (const { name, run } of tests) {
  await run(); console.log('PASS ' + name);
}
console.log(JSON.stringify({ result: 'PASS', cases: tests.length, node: process.version, network: 'FAKE_ONLY', browser_hardware: 'NOT_PROVEN' }));
