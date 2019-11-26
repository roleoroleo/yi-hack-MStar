var APP = APP || {};

APP.maintenance = (function ($) {

    var timeoutVar;

    function init() {
        registerEventHandler();
        setStatus("Camera is online.");
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function (e) {
            saveConfig();
        });
        $(document).on("click", '#button-load', function (e) {
            loadConfig();
        });
        $(document).on("click", '#button-reboot', function (e) {
            rebootCamera();
        });
    }

    function saveConfig() {
        $('#button-save').attr("disabled", true);
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'cgi-bin/save.sh', true);
        xhr.responseType = 'blob';
        xhr.onload = function(e) {
            if (xhr.status == 200) {
                var myBlob = xhr.response;
                var url = URL.createObjectURL(myBlob);
                var $a = $('<a />', {
                    'href': url,
                    'download': 'config.7z',
                    'text': "click"
                }).hide().appendTo("body")[0].click();
                URL.revokeObjectURL(url);
            }
        };
        xhr.send();
    }

    function loadConfig() {
        $('#button-load').attr("disabled", true);
        var fileSelect = document.getElementById('button-file');
        var files = fileSelect.files;
        var formData = new FormData();

        for (var i = 0; i < files.length; i++) {
            var file = files[i];
            formData.append('files[]', file, file.name);
        }

        var xhr = new XMLHttpRequest();
        xhr.open('POST', 'cgi-bin/load.sh', true);
        xhr.onload = function () {
            if (xhr.status === 200) {
                $('#button-load').attr("disabled", false);
            }
            var myText = xhr.response;
            $('#text-load').text(myText);
        };
        xhr.send(formData);
    }

    function rebootCamera() {
        $('#button-reboot').attr("disabled", true);
        $.ajax({
            type: "GET",
            url: 'cgi-bin/reboot.sh',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $('#button-reboot').attr("disabled", false);
            },
            success: function(data) {
                setStatus("Camera is rebooting...");
                waitForBoot();
            }
        });
    }

    function waitForBoot() {
        setInterval(function(){
            $.ajax({
                url: '/',
                success: function(data) {
                    setStatus("Camera is back online, redirecting to home.");
                    $('#button-reboot').attr("disabled", false);
                    window.location.href="/";
                },
                error: function(data) {
                    setStatus("Waiting for the camera to come back online...");
                },
                timeout: 3000,
            });
        }, 5000);
    }

    function setStatus(text)
    {
        $('input[type="text"][data-key="STATUS"]').prop('value', text);
    }

    return {
        init: init
    };

})(jQuery);
