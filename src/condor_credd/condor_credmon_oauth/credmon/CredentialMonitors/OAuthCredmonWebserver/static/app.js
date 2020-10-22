


$(document).ready(function(){
    // Disable function
    jQuery.fn.extend({
        disable: function(state) {
            return this.each(function() {
                this.disabled = state;
            });
        }
    });
    
    setError = function(message) {
        // Get the error id
        $("#alertmessage").html(message);
        $("#alertmessage").removeClass("alert-success");
        $("#alertmessage").addClass("alert-danger");
        $("#alertmessage").removeClass("invisible");
        
        
    };
    
    setSuccess = function(message) {
        // Get the error id
        $("#alertmessage").html(message);
        $("#alertmessage").removeClass("alert-danger");
        $("#alertmessage").addClass("alert-success");
        $("#alertmessage").removeClass("invisible");
        
    }
    
    
    // When the test button is pushed, perform the test
    
    // Text Box connection
    $("#boxtest").click(function() {
        var button = $(this);
        button.disable(true);
        button.append(" <i id='box_button_busy' class='fa fa-spinner fa-spin'></i>")
      
        // Send the ajax to test the connection
        $.getJSON("/test_box", function( data ) {
            // Check the returned status
            if(data.status == 1) {
                // Everything worked
                if ("sharedLink" in data) {
                    setSuccess(data.statusMessage + ": <a href=\"" + data.sharedLink + "\" target='_blank' >" + data.sharedLink + "</a>");
                } else {
                    setSuccess(data.statusMessage);
                }
            } else {
                // Something failed
                setError(data.statusMessage);
            }
            $('#box_button_busy').remove()
            button.disable(false);
          
          
        })
        .fail(function(jqXHR, textStatus, errorThrown) {
            setError(textStatus);
            $('#box_button_busy').remove()
            button.disable(false);
        })
      
      
    });
    
    
});

