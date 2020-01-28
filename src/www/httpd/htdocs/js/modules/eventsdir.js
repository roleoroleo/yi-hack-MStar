var APP = APP || {};

APP.eventsdir = (function ($) {

    var timeoutVar;

    function init() {
        updateEventsDirPage();
        registerEventHandler();
    }

    function registerEventHandler() {
        $(document).on("click", '.button-primary', function (e) {
            deleteDir();
        });
    }

    function deleteDir() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsdirdel.sh?dir='+event.target.id.substring(14),
            dataType: "json",
            success: function(response) {
                window.location.reload();
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function updateEventsDirPage() {
        html = "<table class=\"u-full-width padded-table\"><tbody>";
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsdir.sh',
            dataType: "json",
            success: function(data) {
                html += "<tr><td><b>Date & time</b></td>";
                html += "<td><b>Directory name</b></td>";
                html += "<td><b>Delete directory</b></td></tr>";
                if (data.records.length == 0) {
                    html += "<tr><td>No events</td><td></td></tr>";
                } else {
                    for (var i = 0; i < data.records.length; i++) {
                        var record = data.records[i];
                        html += "<tr><td>" + record.datetime + "</td>";
                        html += "<td><a href=\"/?page=eventsfile&dirname=" + record.dirname + "\">" + record.dirname + "</a></td>";
                        html += "<td><input class=\"button-primary\" type=\"button\" id=\"button-delete-" + record.dirname + "\" value=\"Delete\"/></td></tr>";
                    }
                }
                html += "</tbody></table>";
                document.getElementById("table-container").innerHTML = html;
            },
            error: function(response) {
                console.log('error', response);
            },
            complete: function () {
                clearTimeout(timeoutVar);
                timeoutVar = setTimeout(updateEventsdirPage, 10000);

            }
        });
    }

    return {
        init: init
    };

})(jQuery);
