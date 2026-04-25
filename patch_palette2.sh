sed -i 's/page.goto(file_path)/page.goto(file_path)\n        page.wait_for_timeout(3500)/g' verification/verify_palette.py
