#include <iostream>
#include <stack>
#include <string>

bool isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/';
}

int precedence(char c) {
    switch(c) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
    }
    return -1;
}

std::string infixToPostfix(const std::string &infix) {
    std::stack<char> operators;
    std::string postfix;

    for (char c : infix) {
        if (std::isdigit(c)) {
            postfix += c;
        } else if (isOperator(c)) {
            while (!operators.empty() && precedence(c) <= precedence(operators.top())) {
                postfix += operators.top();
                operators.pop();
            }
            operators.push(c);
        } else if (c == '(') {
            operators.push(c);
        } else if (c == ')') {
            while (!operators.empty() && operators.top() != '(') {
                postfix += operators.top();
                operators.pop();
            }
            if (!operators.empty() && operators.top() == '(') {
                operators.pop();
            }
        }
    }

    while (!operators.empty()) {
        postfix += operators.top();
        operators.pop();
    }

    return postfix;
}

int main() {
    std::string infix = "3+4*(2-1)";
    std::string postfix = infixToPostfix(infix);
    std::cout << "Infix: " << infix << "\nPostfix: " << postfix << std::endl;
    return 0;
}
