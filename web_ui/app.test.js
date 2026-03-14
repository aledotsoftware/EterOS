const { toggleEterMenu } = require('./app');

describe('toggleEterMenu', () => {
    let menu;
    let trigger;
    let controlCenter;
    let ccTrigger;
    let launcher;
    let launcherTrigger;

    beforeEach(() => {
        // Set up DOM
        document.body.innerHTML = `
            <div id="eter-menu" class="eter-menu">
                <div class="menu-item" tabindex="0">Item 1</div>
            </div>
            <div class="os-logo" tabindex="0"></div>
            <div id="control-center" class="control-center"></div>
            <div id="cc-trigger"></div>
            <div id="launcher" class="launcher"></div>
            <div id="launcher-trigger"></div>
        `;

        menu = document.getElementById('eter-menu');
        trigger = document.querySelector('.os-logo');
        controlCenter = document.getElementById('control-center');
        ccTrigger = document.getElementById('cc-trigger');
        launcher = document.getElementById('launcher');
        launcherTrigger = document.getElementById('launcher-trigger');
    });

    test('should toggle active class on menu', () => {
        toggleEterMenu();
        expect(menu.classList.contains('active')).toBe(true);
        toggleEterMenu();
        expect(menu.classList.contains('active')).toBe(false);
    });

    test('should update aria-expanded on trigger', () => {
        toggleEterMenu();
        expect(trigger.getAttribute('aria-expanded')).toBe('true');
        toggleEterMenu();
        expect(trigger.getAttribute('aria-expanded')).toBe('false');
    });

    test('should stop propagation if event is provided', () => {
        const mockEvent = { stopPropagation: jest.fn() };
        toggleEterMenu(mockEvent);
        expect(mockEvent.stopPropagation).toHaveBeenCalled();
    });

    test('should close control center and launcher when opening menu', () => {
        // Open CC and Launcher
        controlCenter.classList.add('active');
        launcher.classList.add('active');

        toggleEterMenu();

        expect(controlCenter.classList.contains('active')).toBe(false);
        expect(launcher.classList.contains('active')).toBe(false);
        expect(ccTrigger.getAttribute('aria-expanded')).toBe('false');
        expect(launcherTrigger.getAttribute('aria-expanded')).toBe('false');
    });

    test('should focus first menu item when opening', () => {
        const firstItem = menu.querySelector('.menu-item');
        const spy = jest.spyOn(firstItem, 'focus');
        toggleEterMenu();
        expect(spy).toHaveBeenCalled();
    });

    test('should focus trigger when closing', () => {
        menu.classList.add('active');
        // We need to simulate the initial state where menu is active
        trigger.setAttribute('aria-expanded', 'true');

        const spy = jest.spyOn(trigger, 'focus');
        toggleEterMenu();
        expect(spy).toHaveBeenCalled();
    });
});

describe('toggleLauncher', () => {
    let launcher;
    let trigger;
    let search;
    let cc;

    beforeEach(() => {
        document.body.innerHTML = `
            <div id="launcher" class="launcher"></div>
            <div id="launcher-trigger"></div>
            <input id="launcher-search">
            <div id="control-center" class="control-center active"></div>
            <div id="cc-trigger" aria-expanded="true"></div>
            <div class="launcher-item"><span class="tag">app</span></div>
        `;
        launcher = document.getElementById('launcher');
        trigger = document.getElementById('launcher-trigger');
        search = document.getElementById('launcher-search');
        cc = document.getElementById('control-center');

        // Mock filterApps since it's called by toggleLauncher
        window.filterApps = jest.fn();
    });

    test('should toggle active class on launcher', () => {
        const { toggleLauncher } = require('./app');
        toggleLauncher();
        expect(launcher.classList.contains('active')).toBe(true);
        toggleLauncher();
        expect(launcher.classList.contains('active')).toBe(false);
    });

    test('should close control center if open', () => {
        const { toggleLauncher } = require('./app');
        toggleLauncher();
        expect(cc.classList.contains('active')).toBe(false);
    });
});

describe('toggleControlCenter', () => {
    let cc;
    let trigger;
    let launcher;

    beforeEach(() => {
        document.body.innerHTML = `
            <div id="control-center" class="control-center"></div>
            <div id="cc-trigger"></div>
            <div id="launcher" class="launcher active"></div>
            <div id="launcher-trigger" aria-expanded="true"></div>
        `;
        cc = document.getElementById('control-center');
        trigger = document.getElementById('cc-trigger');
        launcher = document.getElementById('launcher');
    });

    test('should toggle active class on control center', () => {
        const { toggleControlCenter } = require('./app');
        toggleControlCenter();
        expect(cc.classList.contains('active')).toBe(true);
        toggleControlCenter();
        expect(cc.classList.contains('active')).toBe(false);
    });

    test('should close launcher if open', () => {
        const { toggleControlCenter } = require('./app');
        toggleControlCenter();
        expect(launcher.classList.contains('active')).toBe(false);
    });
});

describe('Window Management', () => {
    let workspace;

    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="launcher active"></div>
            <div class="os-shell">
                <div class="active-app-name">Escritorio</div>
            </div>
        `;
        workspace = document.getElementById('workspace');
        // Reset zIndexCounter mock if needed, but since it's global in app.js, we might not have direct access.
    });

    test('spawnApp should create a new window element', () => {
        const { spawnApp } = require('./app');
        spawnApp('TestApp', 'native');

        const win = workspace.querySelector('.window');
        expect(win).toBeTruthy();
        expect(win.classList.contains('native-app')).toBe(true);
        expect(win.querySelector('.window-title').textContent).toContain('TestApp');
    });

    test('spawnApp should close launcher', () => {
        const { spawnApp } = require('./app');
        const launcher = document.getElementById('launcher');
        spawnApp('TestApp', 'native');
        expect(launcher.classList.contains('active')).toBe(false);
    });

    test('closeWindow should remove the window element', () => {
        const { spawnApp, closeWindow } = require('./app');
        spawnApp('TestApp', 'native');

        const win = workspace.querySelector('.window');
        const closeBtn = win.querySelector('.control.close');

        closeWindow(closeBtn);
        expect(workspace.querySelector('.window')).toBeNull();
    });

    test('maximizeWindow should toggle maximized class and shell classes', () => {
        const { spawnApp, maximizeWindow } = require('./app');
        spawnApp('TestApp', 'native');

        const win = workspace.querySelector('.window');

        // Mock innerText since jsdom doesn't support it well out-of-the-box for text nodes,
        // it often returns undefined for innerText. So we map innerText to textContent
        Object.defineProperty(win.querySelector('.window-title'), 'innerText', {
            get() { return this.textContent; }
        });

        const maxBtn = win.querySelector('.control.maximize');
        const shell = document.querySelector('.os-shell');

        // Maximize
        maximizeWindow(maxBtn);
        expect(win.classList.contains('maximized')).toBe(true);
        expect(shell.classList.contains('window-full')).toBe(true);

        // Restore
        maximizeWindow(maxBtn);
        expect(win.classList.contains('maximized')).toBe(false);
        expect(shell.classList.contains('window-full')).toBe(false);
    });

    test('minimizeWindow should add minimized class', () => {
        const { spawnApp, minimizeWindow } = require('./app');
        spawnApp('TestApp', 'native');

        const win = workspace.querySelector('.window');
        const minBtn = win.querySelector('.control.minimize');

        minimizeWindow(minBtn);
        expect(win.classList.contains('minimized')).toBe(true);
    });
});

describe('snapWindow', () => {
    let workspace;

    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="launcher"></div>
        `;
        workspace = document.getElementById('workspace');
    });

    test('snapWindow should add snapped class and set dimensions for left-50', () => {
        const { spawnApp, snapWindow } = require('./app');
        spawnApp('TestApp', 'native');

        const win = workspace.querySelector('.window');
        const snapBtn = document.createElement('div');
        win.appendChild(snapBtn);

        // Mock to prevent offset errors
        Object.defineProperty(win.querySelector('.window-title'), 'innerText', {
            get() { return this.textContent; }
        });

        snapWindow(snapBtn, 'left-50');

        expect(win.classList.contains('snapped')).toBe(true);
        expect(win.style.left).toBe('0px');
        expect(win.style.width).toBe('50vw');
    });
});
