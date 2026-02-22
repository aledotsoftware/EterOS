
import os
import time
from playwright.sync_api import sync_playwright

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        # Get absolute path
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"

        print(f"Navigating to: {file_path}")
        page.goto(file_path)

        # Wait for splash screen to disappear
        # The app.js says it takes 2500ms + 600ms transition = 3100ms
        print("Waiting for splash screen to disappear...")
        try:
            # wait up to 5 seconds for it to be removed from DOM
            page.locator("#boot-splash").wait_for(state="detached", timeout=5000)
        except Exception as e:
            print(f"Error waiting for splash screen: {e}")
            page.screenshot(path="verification/error_splash.png")
            browser.close()
            return

        # Check focus
        # Need to wait a tiny bit to ensure JS execution loop has processed the removal and focus
        time.sleep(0.5)

        focused_class = page.evaluate("document.activeElement.className")
        focused_tag = page.evaluate("document.activeElement.tagName")

        print(f"Focused element tag: {focused_tag}")
        print(f"Focused element class: {focused_class}")

        page.screenshot(path="verification/focus_result_before.png")

        # Assert
        if "os-logo" in focused_class:
            print("SUCCESS: Focus is on os-logo")
        else:
            print("FAILURE: Focus is NOT on os-logo")

        browser.close()

if __name__ == "__main__":
    run()
