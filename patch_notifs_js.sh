cat << 'JS_EOF' >> web_ui/app.js

function clearNotifs() {
    const notifList = document.getElementById('notif-list');
    const emptyState = document.getElementById('notif-empty');
    const clearBtn = document.getElementById('clear-notifs');

    if (notifList) notifList.innerHTML = '';
    if (emptyState) emptyState.style.display = 'block';
    if (clearBtn) clearBtn.style.display = 'none';
}
JS_EOF
