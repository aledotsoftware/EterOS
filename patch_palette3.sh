sed -i 's/close_btn.hover(force=True)/close_btn.hover(force=True)\n        page.evaluate("document.querySelector(\x27.control.close\x27).focus()")/g' verification/verify_palette.py
