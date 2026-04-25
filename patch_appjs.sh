sed -i '/function filterApps() {/i let filterTimeout;\nfunction debouncedFilterApps() {\n    clearTimeout(filterTimeout);\n    filterTimeout = setTimeout(filterApps, 300);\n}\n' web_ui/app.js
