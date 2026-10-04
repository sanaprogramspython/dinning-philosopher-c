(() => {
    const countInput = document.getElementById('philosopher-count');
    const strategySelect = document.getElementById('strategy-select');
    const table = document.getElementById('table');
    const philosophersLayer = document.getElementById('philosophers');
    const chopsticksLayer = document.getElementById('chopsticks');
    const startButton = document.getElementById('start-button');
    const pauseButton = document.getElementById('pause-button');
    const status = document.getElementById('simulation-status');
    const stateNames = ['Thinking', 'Hungry', 'Waiting', 'Eating'];
    const strategyDescriptions = [
        ['Naive acquisition', 'Philosophers request their left chopstick first. If every philosopher holds one chopstick and waits for the next, circular wait forms.'],
        ['At most N - 1 contenders', 'The simulation limits how many philosophers can hold resources while requesting a meal, breaking circular wait.'],
        ['Global resource ordering', 'Philosophers request the lower-numbered chopstick first, preventing a cycle in the resource wait order.'],
        ['Arbitrator / waiter', 'A waiter grants both chopsticks together, so a philosopher does not hold one while waiting for another.'],
        ['Asymmetric acquisition', 'Philosophers alternate which chopstick they request first, preventing a uniform circular wait.']
    ];
    let engine;
    let timer = null;

    function updateExplanation(count) {
        const title = document.getElementById('explanation-title');
        const text = document.getElementById('explanation-text');
        const deadlocked = engine.normal_deadlocked() !== 0;
        if (deadlocked) {
            const cycle = [];
            for (let id = 0; id < count; id += 1) {
                const waitingFor = engine.normal_waiting_for(id);
                const owner = waitingFor === -1 ? -1 : engine.normal_chopstick_owner(waitingFor);
                if (waitingFor !== -1 && owner !== -1) {
                    cycle.push(`P${id + 1} holds C${engine.normal_held(id, 0) + 1} and waits for C${waitingFor + 1}, held by P${owner + 1}`);
                }
            }
            title.textContent = 'Deadlock detected';
            text.textContent = `${cycle.join('. ')}. Every philosopher is waiting, so no one can enter the critical section.`;
            return;
        }

        let eating = 0;
        let waiting = 0;
        let hungry = 0;
        const activity = [];
        for (let id = 0; id < count; id += 1) {
            const state = engine.normal_state(id);
            if (state === 3) {
                eating += 1;
                const first = engine.normal_held(id, 0);
                const second = engine.normal_held(id, 1);
                activity.push(`P${id + 1} holds C${first + 1} and C${second + 1} and is eating in the critical section`);
            } else if (state === 2) {
                waiting += 1;
                const held = engine.normal_held(id, 0);
                const requested = engine.normal_waiting_for(id);
                if (held !== -1 && requested !== -1) {
                    const owner = engine.normal_chopstick_owner(requested);
                    activity.push(`P${id + 1} holds C${held + 1} and waits for C${requested + 1}${owner === -1 ? '' : `, held by P${owner + 1}`}`);
                } else if (held !== -1) {
                    activity.push(`P${id + 1} holds C${held + 1} while waiting for an available resource`);
                } else {
                    activity.push(`P${id + 1} is waiting to be admitted to the resource request`);
                }
            } else if (state === 1) {
                hungry += 1;
                activity.push(`P${id + 1} is hungry and requesting resources`);
            }
        }

        const [strategyTitle, strategyText] = strategyDescriptions[strategySelect.selectedIndex] || strategyDescriptions[0];
        if (engine.normal_step_number() === 0) {
            title.textContent = 'Ready to simulate';
            text.textContent = strategyText;
            return;
        }
        if (!engine.normal_running()) {
            title.textContent = 'Simulation paused';
            text.textContent = activity.length ? activity.join('. ') + '.' : `The table is idle under ${strategyTitle}.`;
            return;
        }
        title.textContent = eating > 0 ? `${eating} philosopher${eating === 1 ? '' : 's'} in the critical section` : waiting > 0 ? 'Resource requests are waiting' : hungry > 0 ? 'Philosophers are requesting resources' : strategyTitle;
        text.textContent = activity.length ? `${activity.join('. ')}. ${strategyText}` : strategyText;
    }

    function setStatus(label, state) {
        status.textContent = label;
        status.dataset.state = state;
    }

    function stopTimer() {
        if (timer !== null) window.clearInterval(timer);
        timer = null;
        if (engine) engine.normal_pause();
    }

    function buildTable() {
        stopTimer();
        const count = Math.max(2, Math.min(10, Number(countInput.value) || 5));
        const strategy = strategySelect.selectedIndex;
        countInput.value = String(count);
        engine.normal_init(count, strategy);
        table.dataset.count = String(count);
        philosophersLayer.replaceChildren();
        chopsticksLayer.replaceChildren();

        for (let id = 0; id < count; id += 1) {
            const angle = (2 * Math.PI * id) / count - Math.PI / 2;
            const seat = document.createElement('div');
            seat.className = 'philosopher';
            seat.id = `philosopher-${id}`;
            seat.innerHTML = `<strong>P${id + 1}</strong><span class="state-label">Thinking</span>`;
            seat.style.left = `${50 + Math.cos(angle) * 37}%`;
            seat.style.top = `${50 + Math.sin(angle) * 36}%`;
            philosophersLayer.appendChild(seat);

            const stickAngle = angle + Math.PI / count;
            const stick = document.createElement('div');
            stick.className = 'chopstick';
            stick.id = `chopstick-${id}`;
            stick.dataset.homeLeft = String(50 + Math.cos(stickAngle) * 25);
            stick.dataset.homeTop = String(50 + Math.sin(stickAngle) * 25);
            stick.style.left = `${stick.dataset.homeLeft}%`;
            stick.style.top = `${stick.dataset.homeTop}%`;
            stick.style.setProperty('--angle', `${stickAngle * 180 / Math.PI + 90}deg`);
            chopsticksLayer.appendChild(stick);
        }

        document.getElementById('center-note').textContent = `${count} threads / ${count} chopsticks`;
        document.getElementById('deadlock-status').textContent = 'No deadlock';
        document.getElementById('deadlock-status').classList.remove('is-deadlocked');
        setStatus('READY', 'ready');
        render();
    }

    function render() {
        const count = engine.normal_count();
        let eating = 0;
        let waiting = 0;
        let free = 0;

        for (let id = 0; id < count; id += 1) {
            const state = engine.normal_state(id);
            const seat = document.getElementById(`philosopher-${id}`);
            const label = stateNames[state] || stateNames[0];
            if (state === 2 || state === 1) waiting += 1;
            if (state === 3) eating += 1;
            seat.dataset.state = label.toLowerCase();
            seat.querySelector('.state-label').textContent = label;
            seat.setAttribute('aria-label', `Philosopher ${id + 1}: ${label}`);
        }

        for (let id = 0; id < count; id += 1) {
            const owner = engine.normal_chopstick_owner(id);
            const stick = document.getElementById(`chopstick-${id}`);
            const homeLeft = Number(stick.dataset.homeLeft);
            const homeTop = Number(stick.dataset.homeTop);
            stick.classList.toggle('is-held', owner !== -1);
            if (owner === -1) {
                free += 1;
                stick.style.left = `${homeLeft}%`;
                stick.style.top = `${homeTop}%`;
                stick.textContent = `C${id + 1}`;
                stick.title = `Chopstick ${id + 1}: available`;
            } else {
                const angle = (2 * Math.PI * owner) / count - Math.PI / 2;
                stick.style.left = `${50 + Math.cos(angle) * 29}%`;
                stick.style.top = `${50 + Math.sin(angle) * 27}%`;
                stick.textContent = `C${id + 1} P${owner + 1}`;
                stick.title = `Chopstick ${id + 1}: held by philosopher ${owner + 1}`;
            }
        }

        document.getElementById('eating-count').textContent = String(eating);
        document.getElementById('waiting-count').textContent = String(waiting);
        document.getElementById('free-count').textContent = String(free);
        const deadlockStatus = document.getElementById('deadlock-status');
        const deadlocked = engine.normal_deadlocked() !== 0;
        deadlockStatus.textContent = deadlocked ? 'Deadlock detected' : 'No deadlock';
        deadlockStatus.classList.toggle('is-deadlocked', deadlocked);
        updateExplanation(count);
    }

    function advance() {
        engine.normal_step();
        render();
        if (engine.normal_deadlocked()) {
            stopTimer();
            setStatus('DEADLOCK', 'deadlocked');
        } else {
            setStatus('RUNNING', 'running');
        }
    }

    function startSimulation() {
        if (timer !== null || engine.normal_deadlocked()) return;
        engine.normal_start();
        advance();
        if (!engine.normal_deadlocked()) timer = window.setInterval(advance, 700);
    }

    function pauseSimulation() {
        if (timer === null) return;
        stopTimer();
        setStatus('PAUSED', 'paused');
        render();
    }

    startButton.disabled = true;
    setStatus('LOADING C ENGINE', 'loading');
    fetch('normal_engine.wasm')
        .then((response) => {
            if (!response.ok) throw new Error(`Could not load C engine (${response.status}).`);
            return response.arrayBuffer();
        })
        .then((bytes) => WebAssembly.instantiate(bytes, {}))
        .then(({ instance }) => {
            engine = instance.exports;
            startButton.disabled = false;
            buildTable();
            startButton.addEventListener('click', startSimulation);
            pauseButton.addEventListener('click', pauseSimulation);
            document.getElementById('reset-button').addEventListener('click', buildTable);
            countInput.addEventListener('change', buildTable);
            strategySelect.addEventListener('change', buildTable);
        })
        .catch((error) => {
            setStatus('C ENGINE UNAVAILABLE', 'error');
            document.getElementById('explanation-title').textContent = 'Could not load the C simulation';
            document.getElementById('explanation-text').textContent = `${error.message} Open this project through a local web server so the browser can load the WebAssembly module.`;
        });
})();