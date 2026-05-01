#pragma once
// This file is auto-generated from api\cpp\cbindgen.rs
namespace slint {
/// This enum describes the different types of buttons for a pointer event,
/// typically on a mouse or a pencil.
enum class PointerEventButton {
    /// A button that is none of left, right, middle, back or forward. For example,
    /// this is used for the task button on a mouse with many buttons.
    Other,
    /// The left button.
    Left,
    /// The right button.
    Right,
    /// The center button.
    Middle,
    /// The back button.
    Back,
    /// The forward button.
    Forward,
};
namespace testing {
/// This enum represents the different values for the `accessible-role` property, used to describe the
/// role of an element in the context of assistive technology such as screen readers.
enum class AccessibleRole {
    /// The element isn't accessible.
    None,
    /// The element is a `Button` or behaves like one.
    Button,
    /// The element is a `CheckBox` or behaves like one.
    Checkbox,
    /// The element is a `ComboBox` or behaves like one.
    Combobox,
    /// The element is a `GroupBox` or behaves like one.
    Groupbox,
    /// The element is an `Image` or behaves like one. This is automatically applied to `Image` elements.
    Image,
    /// The element is a `ListView` or behaves like one.
    List,
    /// The element is a `Slider` or behaves like one.
    Slider,
    /// The element is a `SpinBox` or behaves like one.
    Spinbox,
    /// The element is a `Tab` or behaves like one.
    Tab,
    /// The element is similar to the tab bar in a `TabWidget`.
    TabList,
    /// The element is a container for tab content.
    TabPanel,
    /// The role for a `Text` element. This is automatically applied to `Text` elements.
    Text,
    /// The role for a `TableView` or behaves like one.
    Table,
    /// The role for a TreeView or behaves like one. (Not provided yet)
    Tree,
    /// The element is a `ProgressIndicator` or behaves like one.
    ProgressIndicator,
    /// The role for widget with editable text such as a `LineEdit` or a `TextEdit`.
    /// This is automatically applied to `TextInput` elements.
    TextInput,
    /// The element is a `Switch` or behaves like one.
    Switch,
    /// The element is an item in a `ListView`.
    ListItem,
    /// The element is a `RadioButton` or behaves like one.
    RadioButton,
};
}
}

/// This namespace contains constants for each special non-printable key.
///
/// Each constant can be converted to SharedString.
/// The constants are meant to be used with the slint::Window::dispatch_key_press_event() and
/// slint::Window::dispatch_key_release_event() functions.
///
/// Example:
/// ```
/// window.dispatch_key_press_event(slint::platform::key_codes::Tab);
/// ```
namespace slint::platform::key_codes {

/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Backspace = u8"\u0008";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Tab = u8"\u0009";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Return = u8"\u000a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Escape = u8"\u001b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Backtab = u8"\u0019";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Delete = u8"\u007f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Shift = u8"\u0010";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Control = u8"\u0011";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Alt = u8"\u0012";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view AltGr = u8"\u0013";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view CapsLock = u8"\u0014";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view ShiftR = u8"\u0015";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view ControlR = u8"\u0016";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Meta = u8"\u0017";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view MetaR = u8"\u0018";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Space = u8"\u0020";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view UpArrow = u8"\uf700";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view DownArrow = u8"\uf701";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view LeftArrow = u8"\uf702";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view RightArrow = u8"\uf703";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F1 = u8"\uf704";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F2 = u8"\uf705";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F3 = u8"\uf706";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F4 = u8"\uf707";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F5 = u8"\uf708";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F6 = u8"\uf709";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F7 = u8"\uf70a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F8 = u8"\uf70b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F9 = u8"\uf70c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F10 = u8"\uf70d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F11 = u8"\uf70e";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F12 = u8"\uf70f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F13 = u8"\uf710";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F14 = u8"\uf711";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F15 = u8"\uf712";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F16 = u8"\uf713";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F17 = u8"\uf714";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F18 = u8"\uf715";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F19 = u8"\uf716";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F20 = u8"\uf717";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F21 = u8"\uf718";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F22 = u8"\uf719";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F23 = u8"\uf71a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F24 = u8"\uf71b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Insert = u8"\uf727";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Home = u8"\uf729";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view End = u8"\uf72b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view PageUp = u8"\uf72c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view PageDown = u8"\uf72d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view ScrollLock = u8"\uf72f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Pause = u8"\uf730";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view SysReq = u8"\uf731";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Stop = u8"\uf734";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Menu = u8"\uf735";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Back = u8"\uf748";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view A = u8"\u0061";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view B = u8"\u0062";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view C = u8"\u0063";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view D = u8"\u0064";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view E = u8"\u0065";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view F = u8"\u0066";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view G = u8"\u0067";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view H = u8"\u0068";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view I = u8"\u0069";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view J = u8"\u006a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view K = u8"\u006b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view L = u8"\u006c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view M = u8"\u006d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view N = u8"\u006e";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view O = u8"\u006f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view P = u8"\u0070";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Q = u8"\u0071";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view R = u8"\u0072";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view S = u8"\u0073";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view T = u8"\u0074";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view U = u8"\u0075";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view V = u8"\u0076";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view W = u8"\u0077";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view X = u8"\u0078";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Y = u8"\u0079";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Z = u8"\u007a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit0 = u8"\u0030";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit1 = u8"\u0031";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit2 = u8"\u0032";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit3 = u8"\u0033";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit4 = u8"\u0034";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit5 = u8"\u0035";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit6 = u8"\u0036";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit7 = u8"\u0037";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit8 = u8"\u0038";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Digit9 = u8"\u0039";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Circumflex = u8"\u005e";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Exclamation = u8"\u0021";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view DoubleQuote = u8"\u0022";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Hash = u8"\u0023";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Dollar = u8"\u0024";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Percent = u8"\u0025";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Ampersand = u8"\u0026";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Underscore = u8"\u005f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view OpenParen = u8"\u0028";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view CloseParen = u8"\u0029";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Asterisk = u8"\u002a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Plus = u8"\u002b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Pipe = u8"\u007c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view HyphenMinus = u8"\u002d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view OpenCurlyBracket = u8"\u007b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view CloseCurlyBracket = u8"\u007d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Tilde = u8"\u007e";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Colon = u8"\u003a";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Semicolon = u8"\u003b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view LessThan = u8"\u003c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Equals = u8"\u003d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view GreaterThan = u8"\u003e";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view QuestionMark = u8"\u003f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view At = u8"\u0040";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Comma = u8"\u002c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Period = u8"\u002e";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Slash = u8"\u002f";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view BackQuote = u8"\u0060";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view OpenBracket = u8"\u005b";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view BackSlash = u8"\u005c";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view CloseBracket = u8"\u005d";
/// A constant that represents the key code to be used in slint::Window::dispatch_key_press_event()
constexpr std::u8string_view Quote = u8"\u0027";
}
