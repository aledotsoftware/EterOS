import os
import time
from playwright.sync_api import sync_playwright

def verify_palette():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)
        page.wait_for_timeout(3500)

        # Open App
        print("Opening App...")
        page.evaluate("spawnApp('GIMP', 'linux')")

        # Wait for window to appear
        page.wait_for_selector('.window')

        # Focus window close button
        print("Focusing window close button...")
        close_btn = page.locator('.control.close')

        # Hover over window close button to show tooltip
        print("Hovering close button for tooltip...")
        close_btn.hover(force=True)
        page.evaluate("document.querySelector('.control.close').focus()")

        # Wait for tooltip transition (0.2s)
        time.sleep(0.5)

        # Take a screenshot of the window header showing tooltip and focus
        page.locator('.window').screenshot(path="verification/window_close_tooltip.png")

        browser.close()
        print("Verification complete.")

if __name__ == "__main__":
    verify_palette()
