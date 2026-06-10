sed -i 's/uses: actions\/checkout@v4/uses: actions\/checkout@v4\n        env:\n          FORCE_JAVASCRIPT_ACTIONS_TO_NODE24: true/g' .github/workflows/build.yml
