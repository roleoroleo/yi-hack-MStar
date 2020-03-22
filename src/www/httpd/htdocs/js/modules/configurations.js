var APP = APP || {};

APP.configurations = (function ($) {

    function init() {
        registerEventHandler();
        fetchConfigs();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function (e) {
            saveConfigs();
        });
    }

    function fetchConfigs() {
        loadingStatusElem = $('#loading-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=system',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function (key, state) {
                    if(key=="HOSTNAME" || key=="HTTPD_PORT" || key=="RTSP_HIGH_PORT" || key=="RTSP_LOw_PORT" || key=="ONVIF_PORT" || key=="USERNAME")
                        $('input[type="text"][data-key="' + key +'"]').prop('value', state);
                    else if(key=="RTSP_HIGH" || key=="RTSP_HIGH" || key=="ONVIF_HIGH" || key=="ONVIF_LOW")
                        $('select[data-key="' + key +'"]').prop('value', state);
                    else if(key=="PASSWORD")
                        $('input[type="password"][data-key="' + key +'"]').prop('value', state);
                    else
                        $('input[type="checkbox"][data-key="' + key +'"]').prop('checked', state === 'yes');
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

        $('.configs-switch input[type="checkbox"]').each(function () {
            configs[$(this).attr('data-key')] = $(this).prop('checked') ? 'yes' : 'no';
        });

        configs["HOSTNAME"] = $('input[type="text"][data-key="HOSTNAME"]').prop('value');

//        if(!validateHostname(configs["HOSTNAME"]))
//        {
//            saveStatusElem.text("Failed");
//            alert("Hostname not valid!");
//            return;
//        }

        configs["HTTPD_PORT"] = $('input[type="text"][data-key="HTTPD_PORT"]').prop('value');
        configs["RTSP_HIGH_PORT"] = $('input[type="text"][data-key="RTSP_HIGH_PORT"]').prop('value');
        configs["RTSP_LOW_PORT"] = $('input[type="text"][data-key="RTSP_LOW_PORT"]').prop('value');
        configs["ONVIF_PORT"] = $('input[type="text"][data-key="ONVIF_PORT"]').prop('value');
        configs["USERNAME"] = $('input[type="text"][data-key="USERNAME"]').prop('value');
        configs["PASSWORD"] = $('input[type="password"][data-key="PASSWORD"]').prop('value');

        $.ajax({
            type: "POST",
            url: 'cgi-bin/set_configs.sh?conf=system',
            data: configs,
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

    function validateHostname(hostname) {
        return /^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$/.test(hostname);
    }

    return {
        init: init
    };

})(jQuery);
