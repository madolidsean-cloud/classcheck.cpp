#define UNICODE
#define _UNICODE
#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int ClassChoice = 1001;
constexpr int NewClassInput = 1002;
constexpr int AddClassButton = 1003;
constexpr int RemoveClassButton = 1004;
constexpr int IdInput = 1005;
constexpr int NameInput = 1006;
constexpr int GradeInput = 1007;
constexpr int SectionInput = 1008;
constexpr int StatusChoice = 1009;
constexpr int StudentList = 1010;
constexpr int AddStudentButton = 1011;
constexpr int MarkButton = 1012;
constexpr int RemoveStudentButton = 1013;

struct Student {
    std::wstring id;
    std::wstring name;
    std::wstring grade;
    std::wstring section;
    std::wstring status = L"Unmarked";
    std::wstring className;
};

// Class names, grades, and sections are entered by the teacher; there are no
// hard-coded school-specific values. Data currently lives for this run only.
std::vector<std::wstring> classes{L"Class 1"};
std::vector<Student> students;
HWND classChoice = nullptr;
HWND newClassInput = nullptr;
HWND idInput = nullptr;
HWND nameInput = nullptr;
HWND gradeInput = nullptr;
HWND sectionInput = nullptr;
HWND statusChoice = nullptr;
HWND studentList = nullptr;

HWND addControl(HWND parent, const wchar_t *type, const wchar_t *text,
                DWORD style, int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExW(
        0, type, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (control) {
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
    return control;
}

std::wstring controlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

std::wstring currentClass() {
    const LRESULT index = SendMessageW(classChoice, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR || static_cast<size_t>(index) >= classes.size()) {
        return L"";
    }
    return classes[static_cast<size_t>(index)];
}

int selectedStudent() {
    return static_cast<int>(SendMessageW(studentList, LB_GETCURSEL, 0, 0));
}

void refreshList(int selectedIndex = LB_ERR) {
    SendMessageW(studentList, LB_RESETCONTENT, 0, 0);
    const std::wstring activeClass = currentClass();
    int visibleIndex = 0;
    for (const Student &student : students) {
        if (student.className != activeClass) {
            continue;
        }
        const std::wstring row = student.id + L"  |  " + student.name + L"  |  Grade: " +
            (student.grade.empty() ? L"—" : student.grade) + L"  |  Section: " +
            (student.section.empty() ? L"—" : student.section) + L"  |  " + student.status;
        SendMessageW(studentList, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(row.c_str()));
        if (visibleIndex == selectedIndex) {
            SendMessageW(studentList, LB_SETCURSEL, visibleIndex, 0);
        }
        ++visibleIndex;
    }
}

int studentIndexForVisibleRow(int row) {
    if (row == LB_ERR) {
        return -1;
    }
    const std::wstring activeClass = currentClass();
    int visibleIndex = 0;
    for (size_t i = 0; i < students.size(); ++i) {
        if (students[i].className == activeClass) {
            if (visibleIndex == row) {
                return static_cast<int>(i);
            }
            ++visibleIndex;
        }
    }
    return -1;
}

void showNoSelectionMessage(HWND window) {
    MessageBoxW(window, L"Select a student from the list first.", L"ClassCheck",
                MB_OK | MB_ICONINFORMATION);
}

void reloadClasses(const std::wstring &selectName = L"") {
    SendMessageW(classChoice, CB_RESETCONTENT, 0, 0);
    int selected = 0;
    for (size_t i = 0; i < classes.size(); ++i) {
        SendMessageW(classChoice, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(classes[i].c_str()));
        if (classes[i] == selectName) {
            selected = static_cast<int>(i);
        }
    }
    SendMessageW(classChoice, CB_SETCURSEL, selected, 0);
    refreshList();
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        addControl(window, L"STATIC", L"Class:", 0, 20, 20, 55, 24, 0);
        classChoice = addControl(window, L"COMBOBOX", L"",
                                 CBS_DROPDOWNLIST | WS_VSCROLL, 75, 16, 220, 220, ClassChoice);
        newClassInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                                   310, 16, 200, 26, NewClassInput);
        addControl(window, L"BUTTON", L"Add class", BS_PUSHBUTTON,
                   520, 15, 95, 28, AddClassButton);
        addControl(window, L"BUTTON", L"Remove class", BS_PUSHBUTTON,
                   625, 15, 115, 28, RemoveClassButton);

        addControl(window, L"STATIC", L"Student ID:", 0, 20, 62, 75, 24, 0);
        idInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                             100, 58, 160, 26, IdInput);
        addControl(window, L"STATIC", L"Name:", 0, 275, 62, 45, 24, 0);
        nameInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                               325, 58, 210, 26, NameInput);
        addControl(window, L"STATIC", L"Grade:", 0, 550, 62, 50, 24, 0);
        gradeInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                                600, 58, 140, 26, GradeInput);

        addControl(window, L"STATIC", L"Section:", 0, 20, 102, 60, 24, 0);
        sectionInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                                  80, 98, 180, 26, SectionInput);
        addControl(window, L"STATIC", L"Attendance:", 0, 275, 102, 80, 24, 0);
        statusChoice = addControl(window, L"COMBOBOX", L"",
                                  CBS_DROPDOWNLIST | WS_VSCROLL, 360, 98, 175, 160, StatusChoice);
        const wchar_t *statuses[] = {L"Unmarked", L"Present", L"Absent", L"Late", L"Excused"};
        for (const wchar_t *status : statuses) {
            SendMessageW(statusChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(status));
        }
        SendMessageW(statusChoice, CB_SETCURSEL, 0, 0);

        addControl(window, L"BUTTON", L"Add student", BS_PUSHBUTTON,
                   550, 96, 95, 30, AddStudentButton);
        addControl(window, L"BUTTON", L"Set status", BS_PUSHBUTTON,
                   655, 96, 85, 30, MarkButton);
        addControl(window, L"STATIC", L"Students in selected class (ID | Name | Grade | Section | Status):",
                   0, 20, 142, 620, 24, 0);
        studentList = addControl(window, L"LISTBOX", L"",
                                 WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                 20, 168, 720, 340, StudentList);
        addControl(window, L"BUTTON", L"Remove selected student", BS_PUSHBUTTON,
                   20, 520, 180, 30, RemoveStudentButton);
        if (!classChoice || !studentList || !statusChoice) {
            return -1;
        }
        reloadClasses(L"Class 1");
        return 0;
    }
    case WM_COMMAND: {
        const int controlId = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        if (controlId == ClassChoice && notification == CBN_SELCHANGE) {
            refreshList();
            return 0;
        }
        if (controlId == AddClassButton) {
            std::wstring name = controlText(newClassInput);
            if (name.empty()) {
                MessageBoxW(window, L"Enter a name for the class.", L"Missing class name",
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (std::find(classes.begin(), classes.end(), name) != classes.end()) {
                MessageBoxW(window, L"A class with that name already exists.", L"Duplicate class",
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            classes.push_back(name);
            SetWindowTextW(newClassInput, L"");
            reloadClasses(name);
            return 0;
        }
        if (controlId == RemoveClassButton) {
            const std::wstring name = currentClass();
            if (classes.size() <= 1) {
                MessageBoxW(window, L"At least one class must remain.", L"Cannot remove class",
                            MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            const bool hasStudents = std::any_of(students.begin(), students.end(),
                [&name](const Student &student) { return student.className == name; });
            if (hasStudents) {
                MessageBoxW(window, L"Remove or move this class's students before deleting the class.",
                            L"Class is not empty", MB_OK | MB_ICONWARNING);
                return 0;
            }
            classes.erase(std::remove(classes.begin(), classes.end(), name), classes.end());
            reloadClasses(classes.front());
            return 0;
        }
        if (controlId == AddStudentButton) {
            const std::wstring id = controlText(idInput);
            const std::wstring name = controlText(nameInput);
            const std::wstring activeClass = currentClass();
            if (id.empty() || name.empty() || activeClass.empty()) {
                MessageBoxW(window, L"Enter both a student ID and name, and select a class.",
                            L"Missing information", MB_OK | MB_ICONWARNING);
                return 0;
            }
            const bool duplicateId = std::any_of(students.begin(), students.end(),
                [&id](const Student &student) { return student.id == id; });
            if (duplicateId) {
                MessageBoxW(window, L"That student ID is already in use.", L"Duplicate ID",
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            students.push_back({id, name, controlText(gradeInput), controlText(sectionInput),
                                L"Unmarked", activeClass});
            SetWindowTextW(idInput, L"");
            SetWindowTextW(nameInput, L"");
            SetWindowTextW(gradeInput, L"");
            SetWindowTextW(sectionInput, L"");
            refreshList();
            return 0;
        }
        if (controlId == MarkButton) {
            const int index = studentIndexForVisibleRow(selectedStudent());
            if (index < 0) {
                showNoSelectionMessage(window);
                return 0;
            }
            const LRESULT statusIndex = SendMessageW(statusChoice, CB_GETCURSEL, 0, 0);
            if (statusIndex == CB_ERR) {
                return 0;
            }
            const LRESULT length = SendMessageW(statusChoice, CB_GETLBTEXTLEN, statusIndex, 0);
            if (length == CB_ERR) {
                return 0;
            }
            std::wstring status(static_cast<size_t>(length) + 1, L'\0');
            SendMessageW(statusChoice, CB_GETLBTEXT, statusIndex,
                         reinterpret_cast<LPARAM>(status.data()));
            status.resize(static_cast<size_t>(length));
            students[static_cast<size_t>(index)].status = status;
            const int visibleRow = selectedStudent();
            refreshList(visibleRow);
            return 0;
        }
        if (controlId == RemoveStudentButton) {
            const int index = studentIndexForVisibleRow(selectedStudent());
            if (index < 0) {
                showNoSelectionMessage(window);
                return 0;
            }
            students.erase(students.begin() + index);
            refreshList();
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const wchar_t className[] = L"ClassCheckNativeWindow";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&windowClass)) {
        MessageBoxW(nullptr, L"Could not register the application window.", L"ClassCheck",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND window = CreateWindowExW(0, className, L"ClassCheck — Attendance Manager",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  780, 610, nullptr, nullptr, instance, nullptr);
    if (!window) {
        MessageBoxW(nullptr, L"Could not create the application window.", L"ClassCheck",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
