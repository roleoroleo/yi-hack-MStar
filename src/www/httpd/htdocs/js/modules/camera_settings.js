var APP = APP || {};

APP.camera_settings = (function($) {

    function init() {
        registerEventHandler();
        fetchConfigs();
        updatePage();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function(e) {
            saveConfigs();
        });
    }

    function fetchConfigs() {
        loadingStatusElem = $('#loading-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=camera',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function(key, state) {
                    if (key == "SENSITIVITY" || key=="CRUISE")
                        $('select[data-key="' + key + '"]').prop('value', state);
                    else
                        $('input[type="checkbox"][data-key="' + key + '"]').prop('checked', state === 'yes');
                });
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function saveConfigs() {
        var saveStatusElem;
        let configs = {};

        saveStatusElem = $('#save-status');

        saveStatusElem.text("Saving...");

        $('.configs-switch input[type="checkbox"]').each(function() {
            configs[$(this).attr('data-key')] = $(this).prop('checked') ? 'yes' : 'no';
        });

        configs["SENSITIVITY"] = $('select[data-key="SENSITIVITY"]').prop('value');
        configs["CRUISE"] = $('select[data-key="CRUISE"]').prop('value');

        $.ajax({
            type: "GET",
            url: 'cgi-bin/camera_settings.sh?' +
                'save_video_on_motion=' + configs["SAVE_VIDEO_ON_MOTION"] +
                '&sensitivity=' + configs["SENSITIVITY"] +
                '&baby_crying_detect=' + configs["BABY_CRYING_DETECT"] +
                '&led=' + configs["LED"] +
                '&ir=' + configs["IR"] +
                '&rotate=' + configs["ROTATE"] +
                '&cruise=' + configs["CRUISE"] +
                '&switch_on=' + configs["SWITCH_ON"],
            dataType: "json",
            success: function(response) {
                saveStatusElem.text("Saved");
            },
            error: function(response) {
                saveStatusElem.text("Error while saving");
                console.log('error', response);
            }
        });
    }

    function updatePage() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/status.json',
            dataType: "json",
            success: function(data) {
                ptz_enabled = ["h201c", "h305r", "y30", "h307"];
                this_model = data["model_suffix"] || "unknown";
                if (ptz_enabled.includes(this_model)) {
                    var lst = document.querySelectorAll(".ptz");
                    for(var i = 0; i < lst.length; ++i) {
                        lst[i].style.display = 'table-row';
                    }
                } else {
                    var lst = document.querySelectorAll(".ptz");
                    for(var i = 0; i < lst.length; ++i) {
                        lst[i].style.display = 'none';
                    }
                }
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    return {
        init: init
    };

})(jQuery);