var APP = APP || {};

APP.ptz = (function ($) {

    function init() {
        registerEventHandler();
        initPage();
    }

    function registerEventHandler() {
        $(document).on("click", '#img-au', function (e) {
            move('#img-au', 'up');
        });
        $(document).on("click", '#img-al', function (e) {
            move('#img-al', 'left');
        });
        $(document).on("click", '#img-ar', function (e) {
            move('#img-ar', 'right');
        });
        $(document).on("click", '#img-ad', function (e) {
            move('#img-ad', 'down');
        });
    }

    function move(button, dir) {
        $(button).attr("disabled", true);
        $.ajax({
            type: "GET",
            url: 'cgi-bin/ptz.sh?dir='+dir,
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
            }
        });

    }

    function initPage() {
        interval = 1000;

        (function p() {
            jQuery.get('/cgi-bin/snapshot.sh?res=low&base64=yes', function(data) {
                image = document.getElementById('imgSnap');
                image.src = 'data:image/png;base64,' + data;
                image.style = 'width:100%;';
            })
            setTimeout(p, interval);
        })();
    }

    return {
        init: init
    };

})(jQuery);
