// ============================================================================
//  BANK MANAGEMENT SYSTEM  (Console Application)
//  Compile (Windows / MinGW):  g++ bank_management_system.cpp -o bank.exe
//  Compile (Dev-C++ / Code::Blocks): just build & run (C++11 or newer)
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstdlib>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    // Minimal getch() replacement so the program also builds on Linux/macOS
    static int getch() {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        int ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
    }
#endif

using namespace std;

// ----------------------------------------------------------------------------
//  Colors / Settings
// ----------------------------------------------------------------------------
#define CLR_RESET  "\033[0m"
#define CLR_BOLD   "\033[1m"
#define CLR_RED    "\033[31m"
#define CLR_GREEN  "\033[32m"
#define CLR_YELLOW "\033[33m"
#define CLR_CYAN   "\033[36m"
#define CLR_GRAY   "\033[90m"

const string CURRENCY_SYMBOL       = "$";      // change to "PKR " etc. if you want
const double INTEREST_RATE  = 5.0;      // percent
const double MAX_LOAN_LIMIT = 50000.0;  // maximum total loan per account
const double MAX_AMOUNT     = 1000000000.0;
const int    MAX_PIN_ATTEMPTS = 3;
const int    MAX_ACCOUNTS   = 100;

// ----------------------------------------------------------------------------
//  Console / UI helpers
// ----------------------------------------------------------------------------
void enableColors() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    }
#endif
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void waitForKey() {
    cout << CLR_GRAY << "\n  Press any key to continue..." << CLR_RESET;
    getch();
    cout << endl;
}

void showSuccess(const string& msg) { cout << CLR_GREEN  << "\n  [SUCCESS] " << msg << CLR_RESET << endl; }
void showError  (const string& msg) { cout << CLR_RED    << "\n  [ERROR]   " << msg << CLR_RESET << endl; }
void showInfo   (const string& msg) { cout << CLR_YELLOW << "\n  [INFO]    " << msg << CLR_RESET << endl; }

void printHeader(const string& title) {
    cout << CLR_BOLD << CLR_CYAN
         << "\n  ==============================================\n"
         << "   " << title << "\n"
         << "  ==============================================" << CLR_RESET << "\n\n";
}

string money(double v) {
    ostringstream oss;
    oss << CURRENCY_SYMBOL << fixed << setprecision(2) << v;
    return oss.str();
}

string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ----------------------------------------------------------------------------
//  Input helpers (all line based, so the input buffer never gets messed up)
// ----------------------------------------------------------------------------
string readLine(const string& prompt) {
    string s;
    cout << prompt;
    if (!getline(cin, s)) {           // input stream closed
        cout << endl;
        exit(0);
    }
    return trim(s);
}

int readInt(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        istringstream iss(s);
        int v; char extra;
        if (!s.empty() && (iss >> v) && !(iss >> extra)) return v;
        showError("Invalid input. Please enter a whole number.");
    }
}

double readAmount(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        istringstream iss(s);
        double v; char extra;
        if (!s.empty() && (iss >> v) && !(iss >> extra)) {
            if (v <= 0)          showError("Amount must be greater than zero.");
            else if (v > MAX_AMOUNT) showError("Amount is too large.");
            else return v;
        } else {
            showError("Invalid input. Please enter a numeric amount (e.g. 1500 or 250.75).");
        }
    }
}

string readMasked(const string& prompt) {
    string input;
    cout << prompt;
    while (true) {
        int ch = getch();
        if (ch == '\r' || ch == '\n') break;
        if (ch == 0 || ch == 0xE0) { getch(); continue; }       // ignore arrow/function keys
        if (ch == '\b' || ch == 127) {
            if (!input.empty()) { input.erase(input.size() - 1); cout << "\b \b"; }
        } else if (isprint(ch)) {
            input.push_back((char)ch);
            cout << '*';
        }
    }
    cout << endl;
    return input;
}

bool askYesNo(const string& prompt) {
    while (true) {
        string s = readLine(prompt + " (y/n): ");
        if (s == "y" || s == "Y" || s == "yes" || s == "YES") return true;
        if (s == "n" || s == "N" || s == "no"  || s == "NO")  return false;
        showError("Please type 'y' for yes or 'n' for no.");
    }
}

// ----------------------------------------------------------------------------
//  Validation helpers
// ----------------------------------------------------------------------------
bool isValidName(const string& n) {
    if (n.size() < 3) return false;
    bool hasLetter = false;
    for (size_t i = 0; i < n.size(); i++) {
        unsigned char c = (unsigned char)n[i];
        if (isalpha(c)) hasLetter = true;
        else if (c != ' ') return false;
    }
    return hasLetter;
}

string readName(const string& prompt) {
    while (true) {
        string n = readLine(prompt);
        if (isValidName(n)) return n;
        showError("Invalid name. Use letters and spaces only (minimum 3 characters).");
    }
}

// Returns empty string if strong, otherwise a description of what is missing
string passwordProblem(const string& p) {
    if (p.size() < 8 || p.size() > 16) return "Password must be 8 to 16 characters long.";
    bool up = false, low = false, dig = false, spec = false;
    for (size_t i = 0; i < p.size(); i++) {
        unsigned char c = (unsigned char)p[i];
        if (isupper(c)) up = true;
        else if (islower(c)) low = true;
        else if (isdigit(c)) dig = true;
        else spec = true;
    }
    if (!up)   return "Password must contain at least one uppercase letter.";
    if (!low)  return "Password must contain at least one lowercase letter.";
    if (!dig)  return "Password must contain at least one digit.";
    if (!spec) return "Password must contain at least one special character (e.g. @ # $ !).";
    return "";
}

string readNewPassword() {
    while (true) {
        string p = readMasked("  Set Password (8-16 chars, upper+lower+digit+symbol): ");
        string problem = passwordProblem(p);
        if (!problem.empty()) { showError(problem); continue; }
        string again = readMasked("  Confirm Password: ");
        if (again != p) { showError("Passwords do not match. Please try again."); continue; }
        return p;
    }
}

string readNewPin() {
    while (true) {
        string p = readMasked("  Set 4-digit PIN: ");
        if (p.size() != 4 || p.find_first_not_of("0123456789") != string::npos) {
            showError("PIN must be exactly 4 digits (0-9).");
            continue;
        }
        string again = readMasked("  Confirm PIN: ");
        if (again != p) { showError("PINs do not match. Please try again."); continue; }
        return p;
    }
}

string readNewRecoveryCode() {
    while (true) {
        string c = readMasked("  Set Recovery Code (minimum 6 characters): ");
        if (c.size() < 6 || c.size() > 20) {
            showError("Recovery code must be 6 to 20 characters long.");
            continue;
        }
        string again = readMasked("  Confirm Recovery Code: ");
        if (again != c) { showError("Recovery codes do not match. Please try again."); continue; }
        return c;
    }
}

string generateAccountNumber() {
    static int counter = 100000;
    counter++;
    ostringstream oss;
    oss << "B-" << counter << "-2025";
    return oss.str();
}

// ----------------------------------------------------------------------------
//  Bank account
// ----------------------------------------------------------------------------
class Bank {
private:
    string password_, pin_, recoveryCode_;
    bool   frozen_;
    double loan_;
    int    failedAttempts_;

public:
    string name, accountNo;
    double balance;

    Bank() : frozen_(false), loan_(0.0), failedAttempts_(0), balance(0.0) {}

    bool isFrozen() const { return frozen_; }

    // ---------- creation ----------
    void create(const string& newAccNo) {
        printHeader("CREATE NEW ACCOUNT");
        name = readName("  Full Name: ");
        accountNo = newAccNo;
        cout << "  Your Account Number : " << CLR_BOLD << accountNo << CLR_RESET
             << "  (please note it down)\n";
        balance = readAmount("  Initial Deposit: ");
        cout << endl;
        password_     = readNewPassword();
        pin_          = readNewPin();
        recoveryCode_ = readNewRecoveryCode();
        showSuccess("Account created successfully. Welcome, " + name + "!");
    }

    // ---------- display ----------
    void show() const {
        printHeader("ACCOUNT DETAILS");
        cout << "  Account Holder : " << name << "\n"
             << "  Account No     : " << accountNo << "\n"
             << "  Balance        : " << money(balance) << "\n"
             << "  Loan Amount    : " << money(loan_) << "\n"
             << "  Status         : "
             << (frozen_ ? string(CLR_RED) + "Frozen" : string(CLR_GREEN) + "Active")
             << CLR_RESET << "\n";
    }

    void showRow() const {
        cout << "  " << left << setw(16) << accountNo
             << setw(22) << name.substr(0, 20)
             << right << setw(14) << money(balance)
             << setw(14) << money(loan_) << "   "
             << (frozen_ ? string(CLR_RED) + "Frozen" : string(CLR_GREEN) + "Active")
             << CLR_RESET << "\n";
    }

    // ---------- authentication ----------
    bool verifyPin() {
        if (frozen_) {
            showError("This account is frozen. Use 'Recover Account' (Security Features) "
                      "or contact an administrator.");
            return false;
        }
        string in = readMasked("  Enter your 4-digit PIN: ");
        if (in == pin_) { failedAttempts_ = 0; return true; }

        failedAttempts_++;
        if (failedAttempts_ >= MAX_PIN_ATTEMPTS) {
            frozen_ = true;
            showError("Incorrect PIN. Too many failed attempts - your account has been frozen for security.");
        } else {
            showError("Incorrect PIN. Attempts remaining: " + to_string(MAX_PIN_ATTEMPTS - failedAttempts_));
        }
        return false;
    }

    bool verifyPassword() {
        for (int tries = 1; tries <= MAX_PIN_ATTEMPTS; tries++) {
            string in = readMasked("  Enter your account password: ");
            if (in == password_) return true;
            if (tries < MAX_PIN_ATTEMPTS)
                showError("Incorrect password. Attempts remaining: " + to_string(MAX_PIN_ATTEMPTS - tries));
        }
        showError("Incorrect password. Operation cancelled.");
        return false;
    }

    bool verifyRecoveryCode() {
        string in = readMasked("  Enter your Recovery Code: ");
        if (in == recoveryCode_) return true;
        showError("Incorrect recovery code.");
        return false;
    }

    // ---------- transactions ----------
    void deposit(double amount) {
        if (!verifyPin()) return;
        balance += amount;
        showSuccess(money(amount) + " deposited. New balance: " + money(balance));
    }

    void withdraw(double amount) {
        if (!verifyPin()) return;
        if (amount > balance) {
            showError("Insufficient balance. Available: " + money(balance));
            return;
        }
        balance -= amount;
        showSuccess(money(amount) + " withdrawn. New balance: " + money(balance));
    }

    void transfer(vector<Bank>& accounts, double amount) {
        if (!verifyPin()) return;
        if (amount > balance) {
            showError("Insufficient balance. Available: " + money(balance));
            return;
        }
        string target = readLine("  Recipient Account No: ");
        for (size_t i = 0; i < target.size(); i++) target[i] = (char)toupper((unsigned char)target[i]);

        if (target == accountNo) {
            showError("You cannot transfer money to your own account.");
            return;
        }
        for (size_t i = 0; i < accounts.size(); i++) {
            if (accounts[i].accountNo == target) {
                if (accounts[i].frozen_) {
                    showError("The recipient account is frozen and cannot receive funds.");
                    return;
                }
                balance -= amount;
                accounts[i].balance += amount;
                showSuccess(money(amount) + " transferred to " + accounts[i].name +
                            " (" + target + "). New balance: " + money(balance));
                return;
            }
        }
        showError("Recipient account not found. Please check the account number.");
    }

    void checkBalance() {
        if (!verifyPin()) return;
        showInfo("Current balance: " + money(balance));
    }

    // ---------- services ----------
    void applyLoan(double amount) {
        if (!verifyPin()) return;
        if (loan_ + amount > MAX_LOAN_LIMIT) {
            showError("Loan limit exceeded. Maximum total loan is " + money(MAX_LOAN_LIMIT) +
                      ". You can still borrow up to " + money(MAX_LOAN_LIMIT - loan_) + ".");
            return;
        }
        loan_ += amount;
        balance += amount;
        showSuccess("Loan of " + money(amount) + " approved and added to your balance.");
    }

    void repayLoan(double amount) {
        if (!verifyPin()) return;
        if (loan_ <= 0) { showInfo("You have no outstanding loan."); return; }
        if (amount > loan_) {
            showError("Repayment exceeds your outstanding loan of " + money(loan_) + ".");
            return;
        }
        if (amount > balance) {
            showError("Insufficient balance to repay. Available: " + money(balance));
            return;
        }
        balance -= amount;
        loan_ -= amount;
        showSuccess("Repayment successful. Remaining loan: " + money(loan_));
    }

    void addInterest() {
        if (!verifyPin()) return;
        if (balance <= 0) { showInfo("No interest applied because the balance is zero."); return; }
        double interest = balance * INTEREST_RATE / 100.0;
        balance += interest;
        showSuccess("Interest of " + money(interest) + " (" + to_string((int)INTEREST_RATE) +
                    "%) credited. New balance: " + money(balance));
    }

    // ---------- security ----------
    void recover() {
        if (!frozen_) { showInfo("Your account is already active. No recovery needed."); return; }
        if (!verifyRecoveryCode()) return;
        frozen_ = false;
        failedAttempts_ = 0;
        showSuccess("Account recovered. You can use your account again.");
    }

    void resetPassword() {
        showInfo("For your security, identity verification is required.");
        if (!verifyRecoveryCode()) return;
        password_ = readNewPassword();
        showSuccess("Password updated successfully.");
    }

    void updateInfo() {
        if (!verifyPassword()) return;
        name = readName("  New Full Name: ");
        if (askYesNo("  Do you also want to change your password?")) {
            password_ = readNewPassword();
        }
        showSuccess("Account information updated successfully.");
    }

    // Returns true if the account may be removed
    bool confirmClose() {
        if (!verifyPin()) return false;
        if (!verifyRecoveryCode()) return false;
        if (loan_ > 0) {
            showError("Account cannot be closed while a loan of " + money(loan_) + " is outstanding.");
            return false;
        }
        showInfo("Remaining balance of " + money(balance) + " will be paid out to you.");
        if (!askYesNo("  Are you sure you want to permanently close this account?")) {
            showInfo("Account closure cancelled.");
            return false;
        }
        return true;
    }

    // ---------- admin ----------
    void setFrozen(bool f) {
        frozen_ = f;
        if (!f) failedAttempts_ = 0;
    }
};

// ----------------------------------------------------------------------------
//  Admin
// ----------------------------------------------------------------------------
struct Admin {
    string username;
    string password;
    string accessLevel;
};

const Admin ADMINS[] = {
    {"admin1", "admin123", "full-access"},
    {"admin2", "admin456", "read-only"}
};
const int NUM_ADMINS = 2;

bool authenticateAdmin(string& accessLevel) {
    printHeader("ADMIN LOGIN");
    for (int tries = 1; tries <= MAX_PIN_ATTEMPTS; tries++) {
        string user = readLine("  Username: ");
        string pass = readMasked("  Password: ");
        for (int i = 0; i < NUM_ADMINS; i++) {
            if (user == ADMINS[i].username && pass == ADMINS[i].password) {
                accessLevel = ADMINS[i].accessLevel;
                showSuccess("Login successful. Access level: " + accessLevel + ".");
                return true;
            }
        }
        if (tries < MAX_PIN_ATTEMPTS)
            showError("Invalid username or password. Attempts remaining: " + to_string(MAX_PIN_ATTEMPTS - tries));
    }
    showError("Too many failed login attempts. Access denied.");
    return false;
}

// ----------------------------------------------------------------------------
//  Lookup helpers
// ----------------------------------------------------------------------------
int findAccount(const vector<Bank>& accounts, const string& accNo) {
    for (size_t i = 0; i < accounts.size(); i++)
        if (accounts[i].accountNo == accNo) return (int)i;
    return -1;
}

// Asks for an account number; returns index or -1 (and prints an error)
int promptAccount(const vector<Bank>& accounts) {
    string no = readLine("  Account No: ");
    for (size_t i = 0; i < no.size(); i++) no[i] = (char)toupper((unsigned char)no[i]);
    int idx = findAccount(accounts, no);
    if (idx == -1) showError("Account not found. Please check the account number and try again.");
    return idx;
}

// Generic menu: prints options, returns the user's choice (1..N)
int showMenu(const string& title, const vector<string>& options) {
    while (true) {
        clearScreen();
        printHeader(title);
        for (size_t i = 0; i < options.size(); i++)
            cout << "   " << CLR_YELLOW << (i + 1) << "." << CLR_RESET << " " << options[i] << "\n";
        cout << endl;
        int c = readInt("  Enter your choice: ");
        if (c >= 1 && c <= (int)options.size()) return c;
        showError("Invalid choice. Please select a number between 1 and " + to_string(options.size()) + ".");
        waitForKey();
    }
}

// ----------------------------------------------------------------------------
//  Menus
// ----------------------------------------------------------------------------
void accountMenu(vector<Bank>& accounts) {
    while (true) {
        int c = showMenu("ACCOUNT MANAGEMENT",
            {"Create Account", "View Account Details", "Update Account Information",
             "Close Account", "Back to Main Menu"});
        if (c == 5) return;
        clearScreen();

        if (c == 1) {
            if ((int)accounts.size() >= MAX_ACCOUNTS) {
                showError("Account limit reached. No more accounts can be created.");
            } else {
                Bank b;
                b.create(generateAccountNumber());
                accounts.push_back(b);
            }
        } else {
            printHeader(c == 2 ? "VIEW ACCOUNT" : c == 3 ? "UPDATE ACCOUNT" : "CLOSE ACCOUNT");
            int idx = promptAccount(accounts);
            if (idx != -1) {
                if (c == 2) accounts[idx].show();
                else if (c == 3) accounts[idx].updateInfo();
                else if (accounts[idx].confirmClose()) {
                    double payout = accounts[idx].balance;
                    string nm = accounts[idx].name;
                    accounts.erase(accounts.begin() + idx);
                    showSuccess("Account closed. " + nm + ", your payout of " + money(payout) +
                                " is ready for collection.");
                }
            }
        }
        waitForKey();
    }
}

void transactionMenu(vector<Bank>& accounts) {
    while (true) {
        int c = showMenu("TRANSACTIONS",
            {"Deposit Money", "Withdraw Money", "Transfer Money", "Check Balance", "Back to Main Menu"});
        if (c == 5) return;
        clearScreen();
        static const char* titles[] = {"", "DEPOSIT", "WITHDRAW", "TRANSFER", "BALANCE ENQUIRY"};
        printHeader(titles[c]);

        int idx = promptAccount(accounts);
        if (idx != -1) {
            Bank& acc = accounts[idx];
            if (c == 1)      acc.deposit(readAmount("  Amount to deposit: "));
            else if (c == 2) acc.withdraw(readAmount("  Amount to withdraw: "));
            else if (c == 3) acc.transfer(accounts, readAmount("  Amount to transfer: "));
            else             acc.checkBalance();
        }
        waitForKey();
    }
}

void servicesMenu(vector<Bank>& accounts) {
    while (true) {
        int c = showMenu("BANKING SERVICES",
            {"Apply for Loan", "Repay Loan", "Calculate & Add Interest", "Back to Main Menu"});
        if (c == 4) return;
        clearScreen();
        static const char* titles[] = {"", "LOAN APPLICATION", "LOAN REPAYMENT", "INTEREST"};
        printHeader(titles[c]);

        int idx = promptAccount(accounts);
        if (idx != -1) {
            Bank& acc = accounts[idx];
            if (c == 1)      acc.applyLoan(readAmount("  Loan amount: "));
            else if (c == 2) acc.repayLoan(readAmount("  Repayment amount: "));
            else             acc.addInterest();
        }
        waitForKey();
    }
}

void securityMenu(vector<Bank>& accounts) {
    while (true) {
        int c = showMenu("SECURITY FEATURES",
            {"Recover Frozen Account", "Reset Password", "Back to Main Menu"});
        if (c == 3) return;
        clearScreen();
        printHeader(c == 1 ? "ACCOUNT RECOVERY" : "PASSWORD RESET");

        int idx = promptAccount(accounts);
        if (idx != -1) {
            if (c == 1) accounts[idx].recover();
            else        accounts[idx].resetPassword();
        }
        waitForKey();
    }
}

void viewAllAccounts(const vector<Bank>& accounts) {
    printHeader("ALL ACCOUNTS");
    if (accounts.empty()) { showInfo("No accounts have been created yet."); return; }
    cout << CLR_BOLD << "  " << left << setw(16) << "Account No" << setw(22) << "Name"
         << right << setw(14) << "Balance" << setw(14) << "Loan" << "   Status" << CLR_RESET << "\n";
    cout << "  " << string(78, '-') << "\n";
    for (size_t i = 0; i < accounts.size(); i++) accounts[i].showRow();
    cout << "  " << string(78, '-') << "\n";
    cout << "  Total accounts: " << accounts.size() << "\n";
}

void adminMenu(vector<Bank>& accounts, bool& loggedIn, string& level) {
    if (!loggedIn) {
        clearScreen();
        loggedIn = authenticateAdmin(level);
        waitForKey();
        if (!loggedIn) return;
    }

    while (true) {
        bool full = (level == "full-access");
        vector<string> opts;
        opts.push_back("View All Accounts");
        if (full) opts.push_back("Freeze / Unfreeze Account");
        opts.push_back("Logout");
        opts.push_back("Back to Main Menu");

        int c = showMenu("ADMIN PANEL  [" + level + "]", opts);
        if (!full && c >= 2) c++;             // read-only has no freeze option

        if (c == 4) return;                   // back
        clearScreen();

        if (c == 1) {
            viewAllAccounts(accounts);
        } else if (c == 2) {
            printHeader("FREEZE / UNFREEZE ACCOUNT");
            int idx = promptAccount(accounts);
            if (idx != -1) {
                showInfo("Current status of " + accounts[idx].name + ": " +
                         (accounts[idx].isFrozen() ? "Frozen" : "Active"));
                cout << "\n   1. Freeze Account\n   2. Unfreeze Account\n   3. Cancel\n\n";
                int f = readInt("  Enter your choice: ");
                if (f == 1) {
                    accounts[idx].setFrozen(true);
                    showSuccess("Account " + accounts[idx].accountNo + " has been frozen.");
                } else if (f == 2) {
                    accounts[idx].setFrozen(false);
                    showSuccess("Account " + accounts[idx].accountNo + " has been unfrozen.");
                } else if (f == 3) {
                    showInfo("Action cancelled.");
                } else {
                    showError("Invalid choice. No changes were made.");
                }
            }
        } else if (c == 3) {
            loggedIn = false;
            level = "";
            showSuccess("You have been logged out successfully.");
            waitForKey();
            return;
        }
        waitForKey();
    }
}

// ----------------------------------------------------------------------------
//  Main
// ----------------------------------------------------------------------------
int main() {
    enableColors();

    vector<Bank> accounts;
    accounts.reserve(MAX_ACCOUNTS);
    bool adminLoggedIn = false;
    string adminLevel;

    while (true) {
        int choice = showMenu("BANK MANAGEMENT SYSTEM  -  MAIN MENU",
            {"Account Management", "Transactions", "Banking Services",
             "Security Features", "Admin Panel", "Exit"});

        switch (choice) {
            case 1: accountMenu(accounts);     break;
            case 2: transactionMenu(accounts); break;
            case 3: servicesMenu(accounts);    break;
            case 4: securityMenu(accounts);    break;
            case 5: adminMenu(accounts, adminLoggedIn, adminLevel); break;
            case 6:
                clearScreen();
                printHeader("THANK YOU");
                cout << "  Thank you for using the Bank Management System.\n"
                     << "  Have a great day!\n\n";
                return 0;
        }
    }
}