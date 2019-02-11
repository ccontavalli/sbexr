function moveTo(url) {
  // TODO: USE history API instead of just moving, so we
  // can prevent reloading all the JSON stuff?
  // console.log("MOVING TO", url)
  window.location.href = url;
}

function adaptSymbolResult(data) {
  var entries = data["data"];
  if (!entries) return {suggestions: []}

  var touse = [];
  for (var i = 0; i < entries.length; ++i) {
    var value = entries[i].name || entries[i].dir || entries[i].file || "(project root)";

    var kinds = entries[i].kinds;
    for (var j = 0; j < kinds.length; j++) {
      kinds[j]._hash = entries[i].hash;
      // kinds[j]._symbol = entries[i];
      kinds[j]._index = j;
      touse.push({value: value, data: kinds[j]});
    }
  }
  return {suggestions: touse}
}
function adaptFileResult(data) {
  var entries = data["data"];
  if (!entries) return {suggestions: []}

  var touse = [];
  for (var i = 0; i < entries.length; ++i) {
    var value = entries[i].name || entries[i].dir || entries[i].file || "(project root)";
    touse.push({value: value, data: entries[i]});
  }
  return {suggestions: touse}
}
function highlightMatch(suggestion, value) {
  if (!value) return suggestion.value;
  var pattern = new RegExp("(" + value + ")", 'gi');
  var result = suggestion.value
      .replace(pattern, '<strong>$1<\/strong>')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/&lt;(\/?strong)&gt;/g, '<$1>');
  return result;
}
function getApi(type) {
  return "/api/" + globalTag + "/" + type;
}

function AdapterDict(dict, adapter) {
  var _this = this;
  _this.dict = dict;
  if (adapter === undefined)
    adapter = function (value) { return value; };
  _this.adapter = adapter;

  _this.get = function (element) {
    var retval = dict[element];
    var found = true;
    if (retval === undefined) {
      found = false;
      retval = element;
    }
    return _this.adapter(retval, found);
  }
}

function DefaultDict(dict, def) {
  return new AdapterDict(dict, function (element, found) {
    if (!found) return def;
    return element;
  })
}

var humanKind = new AdapterDict({
  "ParmVar": "function parameter",
  "Var": "variable",
  "CXXRecord": "struct or class",
  "CXXMethod": "method",
  "EnumConstant": "enum value"}, function (element, found) { return element.toLowerCase(); });

var humanLinkage = new DefaultDict({
1: "local",
2: "local",
3: "global",
4: "global"
}, "");

var humanAccess = new DefaultDict({
0: "public",
1: "protected",
2: "private"
}, "");


$("#searchbox-symbol").autocomplete({
  deferRequestBy: 200,

  params: function (query) { return JSON.stringify({"q": query}); },
  type: "POST",
  dataType: "json",
  transformResult: adaptSymbolResult,
  serviceUrl: getApi("symbol"),
  triggerSelectOnValidInput: false,
  autoSelectFirst: true,
  showNoSuggestionNotice: true,

  formatResult: function (suggestion, value) {
    var match = highlightMatch(suggestion, value);

    var result = "";
    var kind = suggestion.data;

    result += "<span>" + match + " - <i font='smaller'>" + humanKind.get(kind.kind);
    var linkage = humanLinkage.get(kind.linkage);
    var access = humanAccess.get(kind.access);
    if (linkage || access) {
      result += " (";
      if (linkage)
        result += linkage;
      if (access)
        result += " " + access;
      result += ")";
    }
    result += "</i></span>";

    var defs = kind.defs;
    var decls = kind.decls;

    result += "<ul>";
    if (defs) {
      for (var j = 0; j < defs.length; ++j) {
        var el = defs[j];
        result += " <li>Definition: <a href='" + el.href + "'>" + el.location + "</a></li>";
      }
    }
    if (decls) {
      for (var j = 0; j < decls.length; ++j) {
        var el = decls[j];
        result += " <li>Prototype: <a href='" + el.href + "'>" + el.location + "</a></li>";
      }
    }
    result += "</ul>";
    return result;
  },
  onSelect: function (suggestion) {
    url = "../symbol/" + suggestion.data._hash + "#SL" + suggestion.data._index;
    moveTo(url);
  }
});
$("#searchbox-file").autocomplete({
  params: function (query) { return JSON.stringify({"q": query}); },
  type: "POST",
  dataType: "json",
  transformResult: adaptFileResult,
  serviceUrl: getApi("tree"),
  showNoSuggestionNotice: true,

  deferRequestBy: 10,
  lookupLimit: 100,
  minChars: 1,
  onSelect: function (suggestion) {
    moveTo(suggestion.data.href);
  }
});

function keyHandler(state, searchbox, about, help) {
  var _this = this;

  _this.globalState = state;
  _this.searchbox = searchbox;
  _this.about = about;
  _this.help = help;

  _this.setState = function(state) {
    if (_this.globalState && _this.globalState.onExit) _this.globalState.onExit(_this);
    if (state && state.onEnter) state.onEnter(_this);
    _this.globalState = state;
  };

  _this.isRelevant = function(e) {
    var active = document.activeElement;
    if (active.type) return false;
    return true;
  };

  _this.handlePress = function(e) {
    // console.log("received event", e, _this, _this.globalState);
    if (!_this.globalState || !_this.globalState.onPress) return;
    return _this.globalState.onPress(_this, e);
  };

  $(document).keypress(_this.handlePress);
};

// This relies on the left column of the page containing a <span>123</span>
// per line number (123, in this example).
// It find the span, which is supposedly at the same height as the line
// it matches, and returns an enclosing box.
function getLinePosition(line) {
  var lh = $("#line-number")[0].childNodes;
  if (line > lh.length)
    line = lh.length;

  var element = lh[line - 1];
  var brect = document.body.getBoundingClientRect();
  var erect = element.getBoundingClientRect();

  return {top: erect.top - brect.top, bottom: erect.bottom - brect.bottom, left: erect.left - brect.left, right: erect.rigth - brect.right};
};

function hasLineNumbers() {
  return ($("#line-number")[0]) ? true : false;
}

function highlightLine(line) {
  var position = getLinePosition(line);
  return $("<span class='code js-highlight'>&nbsp;</span>").css({top: position.top + "px", left: position.left + "px"}).appendTo("body");
}

var startState = {
  onPress: function(handler, e) {
    if (!handler.isRelevant(e)) return;
    // console.log("keypress", e.which, e);

    switch (e.which) {
      case 63: // '?'
        handler.help.toggle();
        break;
      case 97: // 'a'
        handler.about.toggle();
        break;

      case 58: // ':'
        if (hasLineNumbers())
          handler.searchbox.pushBox("line");
        break;
      case 102: // 'f'
        handler.searchbox.pushBox("file");
        break;
      case 115: // 's'
        handler.searchbox.pushBox("symbol");
        break;
      case 116: // 't'
        handler.searchbox.pushBox("text");
        break;
      case 103: // 'g'
        window.scrollTo(0, 0);
        break;
      case 71: // 'G'
        window.scrollTo(0,document.body.scrollHeight);
        break;
      case 106: // 'j' down
        var tp = document.documentElement.scrollTop || document.body.scrollTop;
        window.scrollTo(0, tp + $(window).height() * 0.10);
        break;
      case 107: // 'k' up
        var tp = document.documentElement.scrollTop || document.body.scrollTop;
        window.scrollTo(0, tp - $(window).height() * 0.10);
        break;

      default:
        return;
    }
    e.preventDefault();
  },
};

function tagPicker() {
  var _this = this;

  if (globalAllTags.length <= 0) return;

  var tagmenu = $("#tag-menu");
  var tagentries = $("#tag-entries");
  var caret = $("<span class='caret'></span>");
  tagentries.empty();
  
  for (var i = 0; i < globalAllTags.length; i++) {
    var tagspan = $("<span class='data-label'></span>");
    var tagentry = $("<li><input type='radio'><label></label></li>");
    var tag = globalAllTags[i];
    var el = tagentry.find("input");
    el.attr("id", "etr-" + i);
    el.attr("name", "tp");
    el.attr("value", tag);
    el = tagentry.find("label");
    el.attr("for", "etr-" + i);
    if (globalProject)
      el.text(globalProject + " ");
    tagspan.text(tag);
    tagspan.appendTo(el);
    tagentry.appendTo(tagentries);
  }
  
  var def = $("#tag-default");
  def.text(globalTag);
  caret.appendTo(def);

  _this._moveif = function(path, onerror) {
    $.ajax({
        type: 'HEAD',
        url: path,
        success: function() {
          moveTo(path);
        },  
        error: function() {
          if (onerror) onerror();
        }
    });
  };

  $("input[name=tp]").change(function (ev) {
    var tag = $("input[name=tp]:checked").attr("value");
    var path = window.location.pathname.split("/"); 
    var newsame = "../../../" + tag + "/sources/" +
          path[path.length - 2] + "/" + path[path.length - 1];
    var newbase = "../../../" + tag + "/sources/meta/index.html";

    _this._moveif(newsame, function () {
      _this._moveif(newbase);
    });
  });

  tagmenu.removeClass("hidden");
};

function searchHandler() {
  var _this = this;

  _this._old = "symbol";
  _this._current = "symbol";

  _this.getSearchboxId = function(value) { return "#searchbox-" + value; };
  _this.getRadioId = function(value) { return "#sb-" + value; };

  // Changes the searchbox as if the user selected it
  // from the radio button, and remembers the one used.
  _this.pushBox = function (newValue, focus) {
    _this._old = _this._current;
    _this.changeBox(newValue, focus);
    return _this._old;
  }
  // Changes back the searchbox to the one that was previously
  // selected by the user.
  _this.popBox = function (focus) {
    return _this.changeBox(_this._old, focus);
  }
  // Forgets the old choice of box.
  _this.flush = function (newValue, focus) {
    _this._old = _this._current;
  }
  // Emulates the action of an user picking a different
  // radio button to change the searchbox.
  _this.changeBox = function (newValue, focus) {
    $(_this.getRadioId(newValue)).prop("checked", true).trigger("change", [true, focus]);
  }
  // Actually changes the searchbox to the one named newValue,
  // optionally focusing on it.
  _this._setBox = function (newValue, focus) {
    var focus = typeof focus !== 'undefined' ?  focus : true;
    var oldValue = _this._current;
    $(_this.getSearchboxId(oldValue)).addClass("hidden");

    var element = $(_this.getSearchboxId(newValue)).removeClass("hidden");
    if (focus)
      element.focus(function() { $(this).select(); }).focus();

    _this._current = newValue;
    return oldValue;
  }

  _this.actions = {
    "symbol": function (ev) { console.log("symbol", ev); },
    "file"  : function (ev) { console.log("file", ev);   },
    "line"  : function (el) {
      var line = parseInt(el.val());
      var rect = getLinePosition(line);

      window.scrollTo(0, rect.top - 70);
      highlightLine(line).fadeOut(2000, function(){ $(this).remove();});
    },
    "text"  : function (ev) { console.log("text", ev);   },
  };
  for (var value in _this.actions) {
    var action = _this.actions[value];
    var element = $(_this.getSearchboxId(value));
    var createHandler = function(el, act) {
      return function (ev) {
        if (ev.which == 13) { act(el); ev.preventDefault(); el.blur(); _this.popBox(false); }
      };
    };
    element.on("keypress", createHandler(element, action));
  }

  // Based on what a user decides to search, update the placeholder
  // in the search box, and associated action.
  $("input[name=sb]").change(function (ev, internal, focus) {
    var newValue = $("input[name=sb]:checked").attr("value");
    _this._setBox(newValue, focus);
    if (!internal) _this.flush();
  });
};
function showAbout() {
  var cookiename = "sbexrBrowsed";
  var cookievalue = Cookies.get(cookiename);
  if (cookievalue && Number(cookievalue) != NaN)
    cookievalue = Number(cookievalue);
  else
    cookievalue = 0;
  Cookies.set(cookiename, cookievalue + 1);
  if (cookievalue < 1)
    about.toggle();
}

var beta = $("<div class='alert alert-danger' role='alert' id='beta'>BETA - <a href='https://groups.google.com/forum/#!forum/sbexr/join'>join group for details</a></div>").appendTo("body");
function showNews() {
  var cookie = "sbexrNews";
  var showNewsFor = 300;
  var fetchNewsFor = 3600;

  var config = Cookies.getJSON(cookie);
  if (!config)
    config = {fetched: 0, displayed: 0, last: 0, previous: 0}
  // console.log("CONFIG", config);

  var inDisplayWindow = false;
  if (config.displayed > ((new Date() / 1000) - showNewsFor))
    inDisplayWindow = true;

  // console.log("DISPLAY WINDOW", inDisplayWindow);

  // If news were fetched at least once in the last hour,
  // don't fetch them again.
  if (!inDisplayWindow && config.fetched >= ((new Date() / 1000) - fetchNewsFor)) {
    if (config.previous != config.last) {
      config.previous = config.last;
      Cookies.set(cookie, config);
    }
    return;
  }

  $.get("../../static/news.json", function (news) {
    // console.log("GOT", config, news.news);
    var toappend = "";
    var limit = 7;
    for (var i = 0; i < news.news.length && i < limit; i++) {
      time = news.news[i].time;
      if ((inDisplayWindow && time <= config.previous) ||
          (!inDisplayWindow && time <= config.last))
        continue;
      toappend += "<li>" + news.news[i].message + "</li>";
    }
  
    if (toappend) {
      beta.append(" - NEWS: <ul>" + toappend + "</ul>")
      if (!inDisplayWindow)
        config.displayed = new Date() / 1000;
    }
  
    config.fetched = new Date() / 1000;
    if (!inDisplayWindow) config.previous = config.last;
    config.last = news.news[0].time;
    Cookies.set(cookie, config);
  });
};

var about = $("#sbexr-about").animatedModal({animatedIn: "bounceInRight", animatedOut: "bounceOutRight"});
var help = $("#sbexr-help").animatedModal({animatedIn: "bounceInRight", animatedOut: "bounceOutRight"});

// Based on characters typed by the user, changes the behavior
// of the page.
var searchhandler = new searchHandler();
var keyhandler = new keyHandler(startState, searchhandler, about, help);
var tagpickerhander = new tagPicker();

// Show the about screen on first access.
showAbout();
showNews();
