const fs = require('fs');
let code = fs.readFileSync('web_ui/app.js', 'utf8');

code = code.replace(
  '<span class="window-title">${name} ${customContent ? \'\' : \'(\' + type.toUpperCase() + \')\'}</span>',
  '<span class="window-title">${name.replace(/</g, "&lt;").replace(/>/g, "&gt;")} ${customContent ? \'\' : \'(\' + type.toUpperCase() + \')\'}</span>'
);

fs.writeFileSync('web_ui/app.js', code);
