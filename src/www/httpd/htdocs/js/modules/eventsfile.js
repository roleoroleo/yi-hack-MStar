var APP = APP || {};

function getUrlVar(key)
{
    var hash;
    var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    for(var i = 0; i < hashes.length; i++)
    {
        hash = hashes[i].split('=');
        if (hash[0] == key)
            return hash[1];
    }
    return '';
}

APP.eventsfile = (function ($) {

    var timeoutVar;

    function init() {
        updateEventsfilePage();
    }

    function updateEventsfilePage() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsfile.sh?dirname=' + getUrlVar('dirname'),
            dataType: "json",
            success: function(data) {
                document.getElementById("title-container").innerHTML = data.date + " - Select event";
                html = "<table class=\"u-full-width padded-table\"><tbody>";
                html += "<tr><td><b>Time</b></td>";
                html += "<td><b>File name</b></td></tr>";
                if (data.records.length == 0) {
                    html += "<tr><td>No events in this folder</td><td></td></tr>";
                } else {
                    for (var i = 0; i < data.records.length; i++) {
                        var record = data.records[i];
                        html += "<tr><td>" + record.time + "</td>";
                        html += "<td><a href=\"record/" + getUrlVar('dirname') + "/" + record.filename + "\">" +  record.filename + "</a></td></tr>";
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
                timeoutVar = setTimeout(updateEventsfilePage, 10000);

            }
        });
    }

    return {
        init: init
    };

})(jQuery);
