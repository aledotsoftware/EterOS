from playwright.sync_api import sync_playwright
import os

def run():
    print("Starting verification script...")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        # Load the local HTML file
        file_path = f"file://{os.getcwd()}/web_ui/index.html"
        print(f"Loading {file_path}")
        page.goto(file_path)

        # Click the launcher button in the dock
        # It's the first dock-item with the menu icon
        print("Clicking launcher...")
        page.locator(".dock-item").first.click()

        # Wait for launcher to be active
        print("Waiting for launcher...")
        page.wait_for_selector(".launcher.active")

        # Type a query that yields no results
        print("Typing search query...")
        search_input = page.locator("#launcher-search")
        search_input.fill("xyz_no_result")

        # Wait for the empty state to appear
        print("Waiting for empty state...")
        empty_state = page.locator("#launcher-empty")
        empty_state.wait_for(state="visible")

        # Take a screenshot
        screenshot_path = "verification/empty_state.png"
        page.screenshot(path=screenshot_path)
        print(f"Screenshot saved to {screenshot_path}")

        browser.close()

if __name__ == "__main__":
    run()
