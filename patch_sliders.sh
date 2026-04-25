for file in verification/verify_slider_*.py verification/verify_sliders.py verification/verify_ux.py; do
    sed -i 's/aria-valuetext/data-value/g' "$file"
    sed -i 's/expect(slider).to_have_attribute("data-value", "80%")/expect(slider).to_have_attribute("value", "80")/g' "$file"
    sed -i 's/expect(slider).to_have_attribute("data-value", "20%")/expect(slider).to_have_attribute("value", "20")/g' "$file"
    sed -i 's/value_display.text_content()/value_display.evaluate("el => el.textContent")/g' "$file"
done
