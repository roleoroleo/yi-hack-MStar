var APP = APP || {};

APP.maintenance = (function($) {

    var timeoutVar;

    function init() {
        registerEventHandler();
        setRebootStatus("Camera is online.");
        getFwStatus();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function(e) {
            saveConfig();
        });
        $(document).on("click", '#button-load', function(e) {
            loadConfig();
        });
        $(document).on("click", '#button-reboot', function(e) {
            rebootCamera();
        });
        $(document).on("click", '#button-reset', function(e) {
            resetCamera();
        });
        $(document).on("click", '#button-upgrade', function(e) {
            upgradeFirmware();
        });
        $(document).on("click", '#button-umount', function(e) {
            umountSD();
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
        xhr.onload = function() {
            if (xhr.status === 200) {
                $('#button-load').attr("disabled", false);
            }
            var myText = xhr.response;
            $('#text-load').text(myText);
        };
        xhr.send(formData);
    }

    function rebootCamera() {
        var confirmation = confirm("Are you sure you want to reboot?");
        if (confirmation) {
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
                    setRebootStatus("Camera is rebooting.");
                    waitForBoot();
                },
                complete: function() {
                    $('#button-reboot').attr("disabled", false);
                }
            });
        }
    }

    function waitForBoot() {
        setInterval(function() {
            $.ajax({
                url: 'index.html',
                cache: false,
                success: function(data) {
                    setRebootStatus("Camera is back online, redirecting to home.");
                    $('#button-reboot').attr("disabled", false);
                    window.location.href = "index.html";
                },
                error: function(data) {
                    setRebootStatus("Waiting for the camera to come back online.");
                },
                timeout: 3000,
            });
        }, 5000);
    }

    function resetCamera() {
        var confirmation = confirm("Are you sure you want to reset?");
        if (confirmation) {
            $('#button-reset').attr("disabled", true);
            $.ajax({
                type: "GET",
                url: 'cgi-bin/reset.sh',
                dataType: "json",
                error: function(response) {
                    console.log('error', response);
                },
                success: function(data) {
                    setResetStatus("Reset completed, reboot your camera.");
                },
                complete: function() {
                    $('#button-reset').attr("disabled", false);
                }
            });
        }
    }

    function upgradeFirmware() {
        $('#button-upgrade').attr("disabled", true);
        setFwStatus("Firmware download in progress.");
        $.ajax({
            type: "GET",
            url: 'cgi-bin/fw_upgrade.sh?get=upgrade',
            error: function(response) {
                console.log('error', response);
            },
            success: function(response) {
                setFwStatus(response);
                waitForUpgrade();
            },
            complete: function () {
                $('#button-upgrade').attr("disabled", false);
            }
        });
    }

    function waitForUpgrade() {
        setInterval(function() {
            $.ajax({
                url: 'index.html',
                cache: false,
                success: function(data) {
                    setFwStatus("Camera is upgrading.");
                    $('#button-upgrade').attr("disabled", false);
                    window.location.href = "index.html";
                },
                error: function(data) {
                    setFwStatus("Waiting for the camera to come back online...");
                },
                timeout: 3000,
            });
        }, 5000);
    }

    function setRebootStatus(text) {
        $('input[type="text"][data-key="STATUS"]').prop('value', text);
    }

    function setResetStatus(text) {
        $('input[type="text"][data-key="RESET"]').prop('value', text);
    }

    function setFwStatus(text) {
        $('input[type="text"][data-key="FW"]').prop('value', text);
    }

    function getFwStatus() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/fw_upgrade.sh?get=info',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                setFwStatus("Error getting fw info");
            },
            success: function(data) {
                if (data.local_fw) {
                    setFwStatus("Installed: " + data.fw_version + " - Available: local SD");
                } else {
                    setFwStatus("Installed: " + data.fw_version + " - Available: " + data.latest_fw);
                }
                if ((data.fw_version == data.latest_fw) && (!data.local_fw)) {
                    $('#button-upgrade').attr("disabled", true);
                }
            }
        });
    }

    function umountSD() {
        $('#button-umount').attr("disabled", true);
        $('input[type="text"][data-key="SD"]').prop('value', "Umount SD in progress...");
        $.ajax({
            type: "GET",
            url: 'cgi-bin/umount_sd.sh',
            error: function(response) {
                console.log('error', response);
                $('#button-umount').attr("disabled", false);
                $('input[type="text"][data-key="SD"]').prop('value', "Failed");
            },
            success: function(response) {
                $('#button-umount').attr("disabled", false);
                if (response.error == "false") {
                    $('input[type="text"][data-key="SD"]').prop('value', "Success, reboot the cam.");
                } else {
                    console.log('error', response);
                    $('input[type="text"][data-key="SD"]').prop('value', "Failed");
                }
            }
        });
    }

    return {
        init: init
    };

})(jQuery);