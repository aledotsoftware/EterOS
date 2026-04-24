import os
import shutil

# Make directories
os.makedirs("/home/jules/verification/screenshots", exist_ok=True)
os.makedirs("/home/jules/verification/videos", exist_ok=True)

from playwright.sync_api import sync_playwright

def run_cuj(page):
    cwd = os.getcwd()
    page.goto(f"file://{cwd}/web_ui/index.html")
    page.wait_for_timeout(3500) # Wait for splash to hide

    # 1. Open launcher
    page.get_by_label("Lanzador de aplicaciones").click()
    page.wait_for_timeout(1000)

    # 2. Type search
    page.get_by_label("Buscar aplicaciones").fill("linux")
    page.wait_for_timeout(1000)

    # Take screenshot of launcher
    page.screenshot(path="/home/jules/verification/screenshots/launcher.png")

    # 3. Open settings app
    page.evaluate("spawnApp('Configuración', 'native')")
    page.wait_for_timeout(1000)

    # Take screenshot of window
    page.screenshot(path="/home/jules/verification/screenshots/window.png")

    # 4. Open Control Center
    page.locator("#cc-trigger").click()
    page.wait_for_timeout(1000)

    # Final Screenshot
    page.screenshot(path="/home/jules/verification/screenshots/verification.png")
    page.wait_for_timeout(1000)

if __name__ == "__main__":
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context(
            record_video_dir="/home/jules/verification/videos"
        )
        page = context.new_page()
        try:
            run_cuj(page)
        finally:
            context.close()
            browser.close()
