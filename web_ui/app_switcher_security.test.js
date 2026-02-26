const app = require('./app');

describe('App Switcher XSS Vulnerability', () => {
    beforeEach(() => {
        document.body.innerHTML = `
            <div id="workspace"></div>
            <div id="launcher" class="active"></div>
            <div id="launcher-trigger"></div>
            <div id="switcher"></div>
            <div id="switcher-list"></div>
        `;
        // Mock requestAnimationFrame for drag logic inside spawnApp
        global.requestAnimationFrame = (cb) => cb();

        // Polyfill innerText for JSDOM as it is not fully implemented
        Object.defineProperty(HTMLElement.prototype, 'innerText', {
            get() {
                return this.textContent;
            },
            configurable: true
        });
    });

    afterEach(() => {
        // cleanup
        delete HTMLElement.prototype.innerText;
    });

    test('should prevent XSS when showing app switcher', () => {
        // 1. Spawn an app with a malicious name
        // Although spawnApp sanitizes the name for the window title,
        // the rendered text content (innerText) will contain the original malicious string.
        const maliciousName = '<img src=x onerror=alert("XSS")>';
        app.spawnApp(maliciousName, 'native');

        // Verify the window title is sanitized initially
        const win = document.querySelector('.window');
        const title = win.querySelector('.window-title');
        // The text content should be the malicious string (decoded entities)
        expect(title.textContent).toContain(maliciousName);
        // But innerHTML should have entities
        expect(title.innerHTML).toContain('&lt;img src=x onerror=alert("XSS")&gt;');
        // And no image tag should be present
        expect(title.querySelector('img')).toBeNull();

        // 2. Trigger the App Switcher (Alt+Q)
        // This invokes showSwitcher(), which reads the title.innerText and puts it into innerHTML
        const event = new KeyboardEvent('keydown', {
            key: 'q',
            altKey: true,
            bubbles: true
        });
        document.dispatchEvent(event);

        console.log('Title textContent:', title.textContent);
        console.log('Title innerHTML:', title.innerHTML);
        console.log('Title innerText:', title.innerText);

        const switcherList = document.getElementById('switcher-list');
        console.log('Switcher List innerHTML:', switcherList.innerHTML);

        // 3. Check if the malicious code was injected into the switcher
        const card = switcherList.querySelector('.switcher-card');

        // If vulnerable, the card will contain an IMG element created from the malicious string
        const injectedImg = card.querySelector('img[onerror]');

        if (injectedImg) {
            console.log('VULNERABILITY CONFIRMED: XSS payload executed in App Switcher');
        } else {
            console.log('SAFE: XSS payload NOT executed in App Switcher');
        }

        // The fix should ensure this is null
        expect(injectedImg).toBeNull(); // Expecting success (vulnerability fixed)
    });
});
