with open(".github/workflows/build.yml", "r") as f:
    content = f.read()

content = content.replace("uses: actions/checkout@v4", "uses: actions/checkout@v4\n        env:\n          FORCE_JAVASCRIPT_ACTIONS_TO_NODE24: true")

with open(".github/workflows/build.yml", "w") as f:
    f.write(content)
