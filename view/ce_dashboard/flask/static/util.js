/**
 * Utility function that binds the "next/previous CE" buttons to the left/right
 * arrow keys.
 */
function registerKeyboardNavigation() {
    $(document).on('keyup', function (e) {
        console.log(e);
        if ((e.keyCode || e.which) == 37) {
            window.location.href = $("#prev-page-link").attr('href');
        } else if ((e.keyCode || e.which) == 39) {
            window.location.href = $("#next-page-link").attr('href');
        }
    });
}
