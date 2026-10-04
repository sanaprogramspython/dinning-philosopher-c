(() => {
    const countInput = document.getElementById('philosopher-count');
    const table = document.getElementById('table');
    const philosophersLayer = document.getElementById('philosophers');
    const chopsticksLayer = document.getElementById('chopsticks');
    const roster = document.getElementById('roster');
    const status = document.getElementById('game-status');
    const message = document.getElementById('game-message');
    const resultPanel = document.getElementById('result-panel');
    const startButton = document.getElementById('start-button');
    const pauseButton = document.getElementById('pause-button');
    const stateNames = ['Thinking', 'Hungry', 'Waiting', 'Eating'];
    const controls = [
        ...document.querySelectorAll('.controls button'),
        ...document.querySelectorAll('.player-controls button'),
        document.getElementById('result-new-game-button')
    ];
    const decoder = new TextDecoder();
    let engine;
    let timer = null;

    function readCString(pointer) {
        const bytes = new Uint8Array(engine.memory.buffer);
        let end = pointer;
        while (bytes[end] !== 0) end += 1;
        return decoder.decode(bytes.subarray(pointer, end));
    }

    function setStatus(label, state) {
        status.textContent = label;
        status.dataset.state = state;
    }

    function stopTimer() {
        if (timer !== null) window.clearInterval(timer);
        timer = null;
    }

    function buildTable() {
        stopTimer();
        const count = Math.max(2, Math.min(10, Number(countInput.value) || 5));
        countInput.value = String(count);
        engine.game_init(count);
        table.dataset.count = String(count);
        philosophersLayer.replaceChildren();
        chopsticksLayer.replaceChildren();

        for (let id = 0; id < count; id += 1) {
            const angle = (2 * Math.PI * id) / count - Math.PI / 2;
            const seat = document.createElement('button');
            seat.type = 'button';
            seat.className = 'philosopher';
            seat.id = `philosopher-${id}`;
            seat.style.left = `${50 + Math.cos(angle) * 37}%`;
            seat.style.top = `${50 + Math.sin(angle) * 36}%`;
            seat.addEventListener('click', () => {
                engine.game_select(id);
                refresh();
            });
            philosophersLayer.append(seat);

            const stickAngle = angle + Math.PI / count;
            const stick = document.createElement('div');
            stick.className = 'chopstick';
            stick.id = `chopstick-${id}`;
            stick.dataset.homeLeft = String(50 + Math.cos(stickAngle) * 25);
            stick.dataset.homeTop = String(50 + Math.sin(stickAngle) * 25);
            stick.style.left = `${stick.dataset.homeLeft}%`;
            stick.style.top = `${stick.dataset.homeTop}%`;
            stick.style.setProperty('--angle', `${stickAngle * 180 / Math.PI + 90}deg`);
            chopsticksLayer.append(stick);
        }

        document.getElementById('center-note').textContent = `${count} seats · clockwise from P1`;
        refresh();
    }

    function render() {
        const count = engine.game_count();
        const selectedId = engine.game_selected();
        const selectedState = engine.game_state(selectedId);
        const running = engine.game_running() !== 0;
        const deadlocked = engine.game_deadlocked() !== 0;
        const gameOver = engine.game_over() !== 0;
        const deadlockOccurred = engine.game_deadlock_occurred() !== 0;
        const selectedHealth = engine.game_health(selectedId);
        let freeCount = 0;

        document.getElementById('score').textContent = String(engine.game_score());
        document.getElementById('time-left').textContent = String(engine.game_time());
        document.getElementById('fed-count').textContent = String(engine.game_fed_count());
        document.getElementById('conflict-count').textContent = String(engine.game_conflicts());
        document.getElementById('selected-label').textContent = `Philosopher ${selectedId + 1}`;
        document.getElementById('selected-state').textContent = `${stateNames[selectedState]} · ${selectedHealth}% health`;

        const deadlockIndicator = document.getElementById('deadlock-indicator');
        deadlockIndicator.textContent = deadlocked ? 'Deadlock detected' : deadlockOccurred ? 'Deadlock occurred' : 'No deadlock';
        deadlockIndicator.classList.toggle('is-deadlocked', deadlocked || deadlockOccurred);

        if (gameOver) setStatus('GAME OVER', 'over');
        else if (deadlocked) setStatus('DEADLOCK', 'deadlocked');
        else if (running) setStatus('RUNNING', 'running');
        else if (engine.game_started()) setStatus('PAUSED', 'paused');
        else setStatus('READY', 'ready');

        const messageType = ['normal', 'warning', 'danger'][engine.game_message_type()] || 'normal';
        message.textContent = readCString(engine.game_message());
        message.dataset.type = messageType;
        startButton.textContent = engine.game_started() ? 'Resume Game' : 'Start Game';

        const canAct = running && !gameOver && !deadlocked;
        document.getElementById('make-hungry-button').disabled = !canAct || selectedState !== 0;
        const leftStick = (selectedId + count - 1) % count;
        const rightStick = selectedId;
        let holdsLeft = false;
        let holdsRight = false;
        for (let slot = 0; slot < engine.game_held_count(selectedId); slot += 1) {
            const stick = engine.game_held(selectedId, slot);
            if (stick === leftStick) holdsLeft = true;
            if (stick === rightStick) holdsRight = true;
        }
        document.getElementById('take-left-button').disabled = !canAct || selectedState === 3 || holdsLeft;
        document.getElementById('take-right-button').disabled = !canAct || selectedState === 3 || holdsRight;
        document.getElementById('take-both-button').disabled = !canAct || selectedState === 0 || selectedState === 3 || engine.game_held_count(selectedId) === 2;

        for (let id = 0; id < count; id += 1) {
            const state = engine.game_state(id);
            const health = engine.game_health(id);
            const fed = engine.game_fed(id) !== 0;
            const seat = document.getElementById(`philosopher-${id}`);
            const level = health <= 25 ? 'low' : health <= 55 ? 'medium' : 'high';
            let detail = 'Ready to work';
            if (state === 3) detail = 'Click again to release';
            else if (fed) detail = 'Fed this round';
            else if (engine.game_waiting_for(id) !== -1) detail = `Waiting for C${engine.game_waiting_for(id) + 1}`;
            else if (engine.game_held_count(id) === 1) detail = `Holding C${engine.game_held(id, 0) + 1}`;
            else if (engine.game_held_count(id) === 2) detail = 'Both chopsticks ready';

            seat.dataset.state = stateNames[state].toLowerCase();
            seat.dataset.fed = String(fed);
            seat.classList.toggle('is-selected', id === selectedId);
            seat.setAttribute('aria-pressed', String(id === selectedId));
            seat.setAttribute('aria-label', `Philosopher ${id + 1}, ${stateNames[state]}, health ${health} percent${fed ? ', fed' : ''}`);
            seat.innerHTML = `<strong>P${id + 1}</strong><span class="state-label">${stateNames[state]}</span><span class="seat-detail">${detail}</span><span class="seat-health" role="progressbar" aria-label="P${id + 1} health" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${health}"><span class="seat-health-fill" data-level="${level}" style="width: ${health}%;"></span></span>`;
        }

        for (let index = 0; index < count; index += 1) {
            const owner = engine.game_chopstick_owner(index);
            const stick = document.getElementById(`chopstick-${index}`);
            const homeLeft = Number(stick.dataset.homeLeft);
            const homeTop = Number(stick.dataset.homeTop);
            const isHeld = owner !== -1;
            const isEating = isHeld && engine.game_state(owner) === 3;
            stick.classList.toggle('is-held', isHeld);
            stick.classList.toggle('is-used', isEating);
            if (!isHeld) {
                freeCount += 1;
                stick.style.left = `${homeLeft}%`;
                stick.style.top = `${homeTop}%`;
                stick.innerHTML = `<span>C${index + 1} FREE</span>`;
                stick.title = `Chopstick ${index + 1}: available`;
            } else {
                const angle = (2 * Math.PI * owner) / count - Math.PI / 2;
                let slot = 1;
                for (let heldSlot = 0; heldSlot < engine.game_held_count(owner); heldSlot += 1) {
                    if (engine.game_held(owner, heldSlot) === index) slot = heldSlot;
                }
                const side = slot === 0 ? -1 : 1;
                stick.style.left = `${50 + Math.cos(angle) * 29 - Math.sin(angle) * side * 2}%`;
                stick.style.top = `${50 + Math.sin(angle) * 26 + Math.cos(angle) * side * 2}%`;
                stick.innerHTML = `<span>C${index + 1} P${owner + 1}${isEating ? ' EAT' : ''}</span>`;
                stick.title = `Chopstick ${index + 1}: ${isEating ? 'being used by' : 'assigned to'} philosopher ${owner + 1}`;
            }
            stick.setAttribute('aria-label', stick.title);
        }

        document.getElementById('free-count').textContent = String(freeCount);
        roster.replaceChildren();
        for (let id = 0; id < count; id += 1) {
            const state = engine.game_state(id);
            const health = engine.game_health(id);
            const fed = engine.game_fed(id) !== 0;
            const level = health <= 25 ? 'low' : health <= 55 ? 'medium' : 'high';
            const row = document.createElement('div');
            row.className = `roster-row${id === selectedId ? ' is-selected' : ''}`;
            row.dataset.state = stateNames[state].toLowerCase();
            row.innerHTML = `<div class="roster-heading"><strong>P${id + 1}${fed ? ' +' : ''}</strong><span class="roster-state">${stateNames[state]}</span></div><div class="health-track" role="progressbar" aria-label="P${id + 1} health" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${health}"><span class="health-fill" data-level="${level}" style="width: ${health}%"></span></div><small>${health}%</small>`;
            roster.append(row);
        }

        resultPanel.hidden = !gameOver;
        if (gameOver) {
            document.getElementById('result-title').textContent = readCString(engine.game_result_title());
            document.getElementById('result-score').textContent = String(engine.game_score());
            document.getElementById('result-fed').textContent = `${engine.game_fed_count()} / ${count}`;
            document.getElementById('result-deadlock').textContent = deadlockOccurred ? 'Yes' : 'No';
            document.getElementById('result-conflicts').textContent = String(engine.game_conflicts());
            document.getElementById('result-feedback').textContent = readCString(engine.game_result_feedback());
        }
    }

    function refresh() {
        if (!engine.game_running()) stopTimer();
        render();
    }

    function startGame() {
        engine.game_start();
        if (engine.game_running() && timer === null) timer = window.setInterval(() => {
            engine.game_tick();
            refresh();
        }, 500);
        refresh();
    }

    function pauseGame() {
        engine.game_pause();
        refresh();
    }

    function act(action) {
        action();
        refresh();
    }

    for (const control of controls) control.disabled = true;
    countInput.disabled = true;
    setStatus('LOADING C ENGINE', 'loading');
    fetch('game_engine.wasm')
        .then((response) => {
            if (!response.ok) throw new Error(`Could not load C engine (${response.status}).`);
            return response.arrayBuffer();
        })
        .then((bytes) => WebAssembly.instantiate(bytes, {}))
        .then(({ instance }) => {
            engine = instance.exports;
            buildTable();
            for (const control of controls) control.disabled = false;
            countInput.disabled = false;
            startButton.addEventListener('click', startGame);
            pauseButton.addEventListener('click', pauseGame);
            document.getElementById('reset-button').addEventListener('click', buildTable);
            document.getElementById('new-game-button').addEventListener('click', buildTable);
            document.getElementById('result-new-game-button').addEventListener('click', buildTable);
            document.getElementById('make-hungry-button').addEventListener('click', () => act(engine.game_make_hungry));
            document.getElementById('take-left-button').addEventListener('click', () => act(engine.game_take_left));
            document.getElementById('take-right-button').addEventListener('click', () => act(engine.game_take_right));
            document.getElementById('take-both-button').addEventListener('click', () => act(engine.game_take_both));
            countInput.addEventListener('change', buildTable);
            refresh();
        })
        .catch((error) => {
            setStatus('C ENGINE UNAVAILABLE', 'over');
            message.textContent = `${error.message} Open this project through a local web server so the browser can load WebAssembly.`;
            message.dataset.type = 'danger';
        });
})();