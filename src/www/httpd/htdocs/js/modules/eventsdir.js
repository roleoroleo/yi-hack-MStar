var APP = APP || {};

APP.eventsdir = (function ($) {

    var timeoutVar;

    function init() {
        updateEventsdirPage();
    }

    function updateEventsdirPage() {
        html = "<table class=\"u-full-width padded-table\"><tbody>";
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsdir.sh',
            dataType: "json",
            success: function(data) {
                html += "<tr><td><b>Date & time</b></td>";
                html += "<td><b>Directory name</b></td></tr>";
                if (data.records.length == 0) {
                    html += "<tr><td>No events</td><td></td></tr>";
                } else {
                    for (var i = 0; i < data.records.length; i++) {
                        var record = data.records[i];
                        html += "<tr><td>" + record.datetime + "</td>";
                        html += "<td><a href=\"/?page=eventsfile&dirname=" + record.dirname + "\">" + record.dirname + "</a></td></tr>";
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
