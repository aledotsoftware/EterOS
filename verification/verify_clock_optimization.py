import os
import re
from playwright.sync_api import sync_playwright, expect

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        cwd = os.getcwd()
        # Ensure we point to the correct file path
        file_path = f"file://{os.path.abspath('web_ui/index.html')}"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Wait for boot splash to disappear (it has a timeout in app.js)
        print("Waiting for boot splash...")
        try:
            # wait for splash to be removed from DOM
            page.locator("#boot-splash").wait_for(state="detached", timeout=5000)
        except:
            print("Splash screen might have been removed already or timed out.")

        # Check Clock
        print("Verifying clock...")
        clock = page.locator("#clock")
        expect(clock).to_be_visible()
        clock_text = clock.text_content()
        print(f"Clock text: {clock_text}")

        # Verify format HH:MM
        if not re.match(r"^\d{2}:\d{2}$", clock_text):
             print(f"WARNING: Clock format unexpected: {clock_text}")

        # Check Aria Label
        print("Verifying aria-label...")
        trigger = page.locator("#cc-trigger")
        aria_label = trigger.get_attribute("aria-label")
        print(f"Aria label: {aria_label}")

        if "Centro de Control" not in aria_label:
             print("ERROR: Aria label missing 'Centro de Control'")
        if clock_text not in aria_label:
             print("ERROR: Aria label missing time")

        # Take screenshot
        print("Taking screenshot...")
        if not os.path.exists("verification"):
            os.makedirs("verification")
        screenshot_path = "verification/clock_verification.png"
        page.screenshot(path=screenshot_path)
        print(f"Screenshot saved to {screenshot_path}")

        browser.close()
        print("Verification complete.")

if __name__ == "__main__":
    run()
