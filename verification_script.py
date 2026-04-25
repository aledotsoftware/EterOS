from playwright.sync_api import sync_playwright
import os

def run():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        cwd = os.getcwd()
        file_path = f"file://{cwd}/web_ui/index.html"
        page.goto(file_path)

        # Wait for splash screen to disappear
        page.evaluate("document.getElementById('boot-splash').remove()")

        page.screenshot(path="final_verification.png")

        browser.close()

if __name__ == "__main__":
    run()
