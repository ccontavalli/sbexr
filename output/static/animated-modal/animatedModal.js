/*=========================================
 * animatedModal.js: Version 1.0
 * author: João Pereira
 * website: http://www.joaopereira.pt
 * email: joaopereirawd@gmail.com
 * Licensed MIT
=========================================*/

(function ($) {
  $.fn.animatedModal = function(options) {
    var modal = $(this);
     
    //Defaults
    var settings = $.extend({
      modalTarget:'animatedModal',
      position:'fixed',
      height:'100%',
      right: '0px',
      zIndexIn: '9999',
      zIndexOut: '-9999',
      color: '#f8f8f8',
      opacityIn:'0.9',
      opacityOut:'0',
      animatedIn:'zoomIn',
      animatedOut:'zoomOut',
      animationDuration:'.6s',
      overflow:'auto',
    }, options);

    var href = modal.attr('href');
    var id = modal.attr('id');
    var added = null;

    var overlay = $("<div></div>");
    overlay.addClass('animated');
    overlay.addClass(settings.modalTarget+'-off');

    //Init styles
    var initStyles = {
      'dispaly': 'hidden',
      'position':settings.position,
      'width':settings.width,
      'height':settings.height,
      'top':settings.top,
      'right':settings.right,
      'background-color':settings.color,
      'overflow-y':settings.overflow,
      'z-index':settings.zIndexOut,
      'opacity':settings.opacityOut,
      '-webkit-animation-duration':settings.animationDuration,
      '-moz-animation-duration':settings.animationDuration,
      '-ms-animation-duration':settings.animationDuration,
      'animation-duration':settings.animationDuration
    };
    //Apply stles
    overlay.css(initStyles);

    var loaded = false;
    var opened = false;

    var close = function(ev) {
      if (ev) ev.preventDefault();

      if (overlay.hasClass(settings.modalTarget+'-on')) {
        overlay.removeClass(settings.modalTarget+'-on');
        overlay.addClass(settings.modalTarget+'-off');
      }
      if (overlay.hasClass(settings.modalTarget+'-off')) {
        overlay.removeClass(settings.animatedIn);
        overlay.addClass(settings.animatedOut);
        overlay.one('webkitAnimationEnd mozAnimationEnd MSAnimationEnd oanimationend animationend', afterClose);
      };
    }

    var show = function(data) {
      modal.addClass("active");

      var css = {}
      var navbar = $('#navbar');
      css.top = navbar.position().top + navbar.outerHeight(true);
      if ($(window).width() < 600) {
        var clickhandler = function (e) {
          console.log("clicked on", e.target)
          if ($(e.target).is("a")) {
            overlay.one('click', clickhandler);
            return;
          }
          close(e);
        };
        overlay.one('click', clickhandler);
        css.width = "100%";
        css["padding-left"] = "10px !important";
      } else {
        css.width = "400px";
      }
      overlay.css(css);

      added = overlay.appendTo("body");

      if (overlay.hasClass(settings.modalTarget+'-off')) {
        overlay.removeClass(settings.animatedOut);
        overlay.removeClass(settings.modalTarget+'-off');
        overlay.addClass(settings.modalTarget+'-on');
      }
      if (overlay.hasClass(settings.modalTarget+'-on')) {
         overlay.css({'opacity':settings.opacityIn,'z-index': settings.zIndexIn});
         overlay.addClass(settings.animatedIn);
         overlay.one('webkitAnimationEnd mozAnimationEnd MSAnimationEnd oanimationend animationend', afterOpen);
      }

      // FIXME: remove escape code handler?
      $(document).on('keyup', function(e) {
        if (e.keyCode == 27) close(e);
      });
    }

    var open = function () {
      if (loaded) {
        opened = true;
        show();
        return;
      }
      $.get(href, function (data) {
        var temp = $('<output>').append($.parseHTML(data));
        var parsed = temp.find("#" + id + "-data");

        parsed.appendTo(overlay);
        loaded = true;
        opened = true;
        show();
      });
    };

    var toggle = function() {
      if (opened) { close(); return; }
      open();
    };


    modal.click(function (ev) {
      if (ev) ev.preventDefault();
      toggle();
    });
    function afterOpen() {
    }
    function afterClose() {
      opened = false;
      added.remove();
      modal.removeClass("active");
      modal.blur();
    }

    return { toggle: toggle };
  };
}(jQuery));
