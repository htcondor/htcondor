// Reset an #anchor references to prevent jumping past content
$(document).ready(function() {
    if (window.location.hash) {
        setTimeout(function () {
            var hash = window.location.hash;
            window.location.replace(hash);
        }, 300);
    }
});