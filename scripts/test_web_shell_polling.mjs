// Local orchestration regression only: no browser, network, packages or board.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(new URL('../lib/fermentation_app/src/web_application_routes.cpp', import.meta.url), 'utf8');
const literal = source.match(/constexpr char kWebShellSuffix\[\] =([\s\S]*?);\n\nstd::string provisioningShell/);
assert.ok(literal, 'extract the actual production suffix');
const suffix = [...literal[1].matchAll(/"(?:[^"\\]|\\.)*"/g)].map(x => JSON.parse(x[0])).join('');
const script = "const S='local-fixture',A='password-protected',U=true;let Q=1" + suffix.split('</script>')[0];
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
  addEventListener() {}
  replaceChildren(...children) {
    this.children = children;
    this.value = children[0]?.value || '';
  }
}

function snapshot(mode = 'active-run', run = '1') {
  return [
    { homeMode: mode, revisions: { run, programCatalog: '1' } },
    { temperatures: [] },
    { alerts: mode === 'active-run' ? [{ id: '1', code: 'test-alert', active: true, acknowledged: false, muted: false, severity: 'warning' }] : [] },
  ];
}

async function fixture() {
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
  h.connection = element('#connection');
  h.board = element('#board');
  const setTimer = (callback, delay, interval = false) => {
    const id = ++timerId;
    h.timers.set(id, { callback, delay, due: h.time + delay, interval });
    return id;
  };
  const context = vm.createContext({
    document: { querySelector: element, createElement: () => new Element(), documentElement: {} },
    navigator: { language: 'en' }, window: { __WEB_CSRF__: csrfToken },
    location: { reload: () => { ++h.reloads; } },
    Option: class extends Element { constructor(name, id) { super(); this.textContent = name; this.value = id; } },
    Date: { now: () => h.time }, AbortController,
    setTimeout: (callback, delay) => setTimer(callback, delay),
    clearTimeout: id => h.timers.delete(id),
    setInterval: (callback, delay) => setTimer(callback, delay, true),
    fetch: (url, options) => {
      if (url === '/api/v1/programs') {
        return Promise.resolve({ ok: true, json: async () => ({ catalogRevision: '1', programs: [{ name: 'Fixture', id: 'p1' }] }) });
      }
      if (url === '/internal/ui/run') {
        const response = deferred(), body = deferred();
        h.posts.push({ url, options, started: h.time, reply: outcome => {
          response.resolve({ status: 200, json: () => body.promise });
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
  vm.runInContext(script, context);
  await drain();
  assert.equal(h.requests.length, 3, 'immediate initial group');
  return h;
}

const tests = [];
function test(name, run) { tests.push({ name, run }); }

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

for (const { name, run } of tests) {
  await run(); console.log('PASS ' + name);
}
console.log(JSON.stringify({ result: 'PASS', cases: tests.length, node: process.version, network: 'FAKE_ONLY', browser_hardware: 'NOT_PROVEN' }));
