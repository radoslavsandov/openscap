function openRuleDetailsDialog(rule_result_id)
{
    $("#result-detail-modal").remove();

    var closebutton = $('<button type="button" class="close" data-dismiss="modal" aria-hidden="true" title="Close">×</button>');
    var modal = $('<div id="result-detail-modal" class="modal fade" tabindex="-1" role="dialog" aria-hidden="true"><div id="result-detail-modal-body" class="modal-body"></div></div>');

    $("body").prepend(modal);

    var resultclone = $("#result-detail-" + rule_result_id).clone();
    resultclone.attr("id", "");
    resultclone.children(".panel-heading").append(closebutton);
    closebutton.css( { marginTop: '-=17px' } );
    $("#result-detail-modal-body").append(resultclone);

    $("#result-detail-modal").modal();

    return false;
}

function toggleRuleDisplay(checkbox)
{
    var result = checkbox.value;

    if (checkbox.checked)
    {
        $(".rule-overview-leaf-" + result).removeClass("rule-result-filtered");
        $(".result-detail-" + result).removeClass("rule-result-filtered");
    }
    else
    {
        $(".rule-overview-leaf-" + result).addClass("rule-result-filtered");
        $(".result-detail-" + result).addClass("rule-result-filtered");
    }
}

function toggleResultDetails(button)
{
    var result_details = $("#result-details");

    if (result_details.is(":visible"))
    {
        result_details.hide();
        $(button).html("Show all result details");
    }
    else
    {
        result_details.show();
        $(button).html("Hide all result details");
    }

    return false;
}

function ruleSearchMatches(detail_leaf, keywords)
{
    if (keywords.length == 0)
        return true;

    var match = true;
    var checked_keywords = detail_leaf.children(".keywords").text().toLowerCase();

    var index;
    for (index = 0; index < keywords.length; ++index)
    {
        if (checked_keywords.indexOf(keywords[index].toLowerCase()) < 0)
        {
            match = false;
            break;
        }
    }

    return match;
}

function ruleSearch()
{
    var search_input = $("#search-input").val();
    var keywords = search_input.split(/[\s,\.;]+/);
    var matches = 0;

    $(".result-detail").each(function(){
        // result-result-rrid, that's offset 14
        var rrid = $(this).attr("id").substring(14);

        var overview_leaf = $("#rule-overview-leaf-" + rrid);
        var detail_leaf = $(this);

        if (ruleSearchMatches(detail_leaf, keywords))
    {
        overview_leaf.removeClass("search-no-match");
        detail_leaf.removeClass("search-no-match");
        ++matches;
    }
        else
    {
        overview_leaf.addClass("search-no-match");
        detail_leaf.addClass("search-no-match");
    }
    });

    if (!search_input)
        $("#search-matches").html("");
    else if (matches > 0)
        $("#search-matches").html(matches.toString() + " rules match.");
    else
        $("#search-matches").html("No rules match your search criteria!");
}

$(document).ready( function() {
    $("#result-details").hide();
    $(".js-only").show();
    $(".toggle-rule-display").each(function(){
        toggleRuleDisplay(this);
    });

    $(".treetable").treetable({ column: 0, expandable: true, initialState : "expanded",  clickableNodeNames : true, indent : 0 });
});
