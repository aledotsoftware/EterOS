import os
import re
from playwright.sync_api import sync_playwright, expect

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the file
        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Wait for boot splash to disappear
        print("Waiting for boot splash...")
        page.locator("#boot-splash").wait_for(state="detached", timeout=5000)

        # Open Control Center
        print("Opening Control Center...")
        page.locator("#cc-trigger").click()

        # Wait for Control Center to be visible/active
        cc = page.locator("#control-center")
        expect(cc).to_have_class(re.compile(r"active"))

        # Verify Slider
        print("Verifying slider...")
        slider = page.locator(".cc-slider").first
        value_display = page.locator(".slider-value").first

        # Initial state
        expect(value_display).to_have_text("80%")
        expect(slider).to_have_attribute("aria-valuenow", "80")

        # Change slider value multiple times rapidly to simulate drag
        print("Simulating rapid slider movement...")
        for val in [70, 60, 50, 40, 30]:
            slider.evaluate(f"el => {{ el.value = {val}; el.dispatchEvent(new Event('input')); }}")
            # We don't wait here to simulate rapid input, but RAF should pick up the last one eventually

        # Wait for RAF to settle (a small delay or expect poll)
        page.wait_for_timeout(100) # 100ms should be enough for RAF

        # Check final value
        expect(value_display).to_have_text("30%")
        expect(slider).to_have_attribute("aria-valuenow", "30")

        # Take screenshot
        print("Taking screenshot...")
        if not os.path.exists("verification"):
            os.makedirs("verification")
        screenshot_path = "verification/slider_raf_verified.png"
        page.screenshot(path=screenshot_path)

        browser.close()
        print(f"Verification complete. Screenshot saved to {screenshot_path}")

if __name__ == "__main__":
    run()
