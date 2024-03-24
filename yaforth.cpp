// yaforth.cpp : This yet another Forth implementation
// 
// With hope that this code useful stuff but without any warranty
//
// This file distributed under Xameleon Green License
//
//
#ifdef _WIN32
#define  _CRT_SECURE_NO_WARNINGS

#include <direct.h>
#include <io.h>
#include <Windows.h>
#include <conio.h>

#define isatty _isatty

#else

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#define _getcwd getcwd

#endif

extern int read_key();

#include "yaforth.h"
#include "memory.h"
#include "generators.h"
#include <stdexcept>

//#define TRACE true

static std::stack<word_t>           integer_stack;
static std::stack<word_t>           return_stack;
static std::stack<word_t>           condition_stack;
static std::vector<word_t>          leave_counts;
static registry_t                   words;
static std::string                  func_name;
static ram_memory                   memory;
static int                          line_no;
static word_t                       base_address;
// Global internal states
options_t   options;

state_t     forth(const char* str);
uint32_t    register_word(std::string &word, word_t address);
state_t     register_variable(std::string &word);
state_t     register_constant(std::string& word);
state_t     execute();

bool is_compile_mode()
{
    return !func_name.empty();
}

record_t* find_record(uint32_t h)
{
    record_t* rec_ptr = nullptr;
    if (words.count(h) > 0)
        rec_ptr = &words[h];
    return rec_ptr;
}

int get_stack_size()
{
    return integer_stack.size();
}

#pragma region Snippets


state_t parse_number(std::string& number_str)
{
    word_t n;
    word_t base = memory.get(base_address);

    int mask = 0;
    switch (base)
    {
    case 2:
        fprintf(stderr, "TODO: input in octal base\n");
        return error;
    case 8:
        sscanf(number_str.c_str(), "%o ", &n);
        break;
    case 10:
        n = std::stol(number_str);
        break;
    case 16:
        sscanf(number_str.c_str(), "%x ", &n);
        break;
    default:
        fprintf(stderr, "Only binary, octal, decimal and hexdecimal numbers allowed in this version\n");
        return error;
    }

    memory.put(hash("^push"));
    memory.put(n);
    number_str.clear();
    return neutral;
}

state_t show_words()
{
    fprintf(stdout, "List of defined words:\n\n");
    for (auto& word : words) {
        fprintf(stdout, "%s ", word.second.NAME.c_str());
    }
    return neutral;
}


state_t register_function()
{
    word_t  w = memory.get();
    return neutral;
}

state_t push_value()
{
    word_t  w = memory.get();
    integer_stack.push(w);
#ifdef TRACE
        printf("%d ", w);
#endif
    return neutral;
}

state_t do_here()
{
    word_t  w = memory.get_current_address();
    integer_stack.push(w);
#ifdef TRACE
    printf("%d ", w);
#endif
    return neutral;
}

state_t do_jump()
{
    word_t  w = memory.get();
    memory.jump(w);
    return neutral;
}

state_t to_R()
{
    word_t n = integer_stack.top();
    integer_stack.pop();
    return_stack.push(n);
    return neutral;
}

state_t from_R()
{
    word_t n = return_stack.top();
    return_stack.pop();
    integer_stack.push(n);
    return neutral;
}

state_t fetch_R()
{
    word_t n = return_stack.top();
    integer_stack.push(n);
    return neutral;
}

state_t do_add()
{
    word_t n1, n2;
    n1 = integer_stack.top();
    integer_stack.pop();
    n2 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 + n2);
    return neutral;
}

state_t do_sub()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 - n2);
    return neutral;
}

state_t do_store()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    memory.set(n2, n1);
    return neutral;
}

state_t do_fetch()
{
    word_t n1;
    n1 = integer_stack.top();
    integer_stack.pop();
    n1 = memory.get(n1);
    integer_stack.push(n1);
    return neutral;
}

state_t do_mul()
{
    word_t n1, n2;
    n1 = integer_stack.top();
    integer_stack.pop();
    n2 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 * n2);
    return neutral;
}

state_t do_div()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 / n2);
    return neutral;
}

state_t check_eq()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 == n2 ? -1 : 0);
    return neutral;
}

state_t check_not_eq()
{
    word_t  n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 != n2 ? -1 : 0);
    return neutral;
}

state_t check_less()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 < n2 ? -1 : 0);
    return neutral;
}

state_t check_more()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(signed(n1) > signed(n2) ? -1 : 0);
    return neutral;
}

state_t op_and()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 & n2);
    return neutral;
}

state_t op_or()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 | n2);
    return neutral;
}

state_t op_xor()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 ^ n2);
    return neutral;
}

state_t op_lshift()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 << n2);
    return neutral;
}

state_t op_rshift()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 >> n2);
    return neutral;
}

state_t invert()
{
    word_t n = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(~n);
    return neutral;
}

state_t mod()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 % n2);
    return neutral;
}

state_t divmod()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1 % n2);
    integer_stack.push(n1 / n2);
    return neutral;
}

state_t dup()
{
    word_t n;
    n = integer_stack.top();
    integer_stack.push(n);
    return neutral;
}

state_t swap()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n2);
    integer_stack.push(n1);
    return neutral;
}

state_t over()
{
    word_t n1, n2;
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n1);
    integer_stack.push(n2);
    integer_stack.push(n1);
    return neutral;
}

state_t rot()
{
    word_t n1, n2, n3;
    n3 = integer_stack.top();
    integer_stack.pop();
    n2 = integer_stack.top();
    integer_stack.pop();
    n1 = integer_stack.top();
    integer_stack.pop();
    integer_stack.push(n2);
    integer_stack.push(n3);
    integer_stack.push(n1);
    return neutral;
}

state_t drop()
{
    integer_stack.pop();
    return neutral;
}

state_t do_dot()
{
    if (integer_stack.empty())
    {
        printf("Stack underflow\n");
        return error;;
    }
    word_t base = memory.get(base_address);
    word_t val = integer_stack.top();
    int mask = 0;
    switch (base)
    {
    case 2:
        mask = 0x80;
        for (int i = 0; i < 8; i++)
        {
            fputc(mask & val ? '1' : '0', stdout);
            mask >>= 1;
        }
        break;
    case 8:
        printf("0%o ", val);
        break;
    case 10:
        printf("%d ", val);
        break;
    case 16:
        printf("0x%x ", val);
        break;
    default:
        fprintf(stderr,"Only binary, octal, decimal and hexdecimal numbers allowed in this version\n");
        return error;
    }

    integer_stack.pop();
    return neutral;
}

state_t get_key()
{
    word_t ch = read_key();
    integer_stack.push(ch);
    return neutral;
}

state_t emit()
{
    word_t  ch = integer_stack.top();
    integer_stack.pop();
    putc(ch, stdout);
    return neutral;
}

state_t do_print_string()
{
    word_t a = memory.get_current_address();
    a += 2;
    word_t len = memory.get(a);
    word_t word_count = (len + 3) / sizeof(word_t);

    char* buffer = (char*)alloca((word_count + 1) << 2);
    word_t* slots = (word_t*)buffer;
    int i;
    for (i = 0; i < word_count; i++)
        slots[i] = memory.get(++a);
    slots[i] = 0;
    buffer[len] = 0;
    printf("%s ", buffer);
    fflush(stdout);

    return neutral;
}


#pragma endregion

state_t do_begin()
{
    word_t  a = memory.get_current_address();
    return_stack.push(a);
    return_stack.push(a);
    return neutral;
}

state_t do_until()
{
    word_t  i, a;

    a = return_stack.top();

    i = integer_stack.top();
    integer_stack.pop();


    if (i == -1 ) {
        return_stack.pop();
        return_stack.pop();
        return neutral;
    }
    memory.jump(a);
    return neutral;

}

state_t do_again()
{
    word_t  a;
    a = return_stack.top();
    memory.jump(a);
    return neutral;
}

state_t do_loop()
{
    word_t max, i;

//    a = memory.get_current_address();

    i = integer_stack.top();
    integer_stack.pop();
    max = integer_stack.top();
    integer_stack.pop();

    return_stack.push(max);
    return_stack.push(i);

    return neutral;
}

state_t prepare_enter_loop(word_t h)
{
    word_t a;
    memory.put(h);
    a = memory.get_current_address();
    return_stack.push(a);     // loop start address
    leave_counts.push_back(0);
    return neutral;
}

state_t prepare_loop_exit(word_t h)
{
    word_t ja = memory.get_current_address() + 2;

    // update leave addresses
    int idx = leave_counts.size();
    if (idx > 0) {
        word_t a;
        --idx;
        int count = leave_counts[idx];
        while(count) {
            a = return_stack.top();
            return_stack.pop();
            memory.set(a, ja);
            --count;
        }
        leave_counts.pop_back();
    }

    word_t end;
    word_t enter_loop = return_stack.top();
    return_stack.pop();
    memory.put(h);
    memory.put(enter_loop);

    end = memory.get_current_address();
    if (end != ja) {
        printf("something going wrong in prepare leave\n");
    }
    return neutral;
}

state_t do_leave()
{
    return_stack.pop();
    return_stack.pop();

    word_t ra = memory.get();
    memory.jump(ra);
    return neutral;
}

state_t prepare_leave()
{
    int idx = leave_counts.size();
    if (idx < 1) {
        printf("Misplaced operator 'leave'\n");
        return error;
    }
    idx--;

    word_t n = hash("^leave");
    memory.put(n);
    word_t a = memory.get_current_address();
    memory.put(0);
    return_stack.push(a);
    leave_counts[idx]++;
    return neutral;
}

state_t end_loop()
{
    word_t a, i, max;

    i = return_stack.top();
    return_stack.pop();
    max = return_stack.top();

    i++;

    if (i >= max) {
        return_stack.pop();
        a = memory.get();
        return neutral;
    }
    return_stack.push(i);
    a = memory.get();
    memory.jump(a);
    return neutral;
}

state_t end_plus_loop()
{
    word_t a, i, max, step;

    step = integer_stack.top();
    integer_stack.pop();

    i = return_stack.top();
    return_stack.pop();
    max = return_stack.top();

    i+= step;

    if (i >= max) {
        return_stack.pop();
        a = memory.get();
        return neutral;
    }
    return_stack.push(i);
    a = memory.get();
    memory.jump(a);
    return neutral;
}

state_t index_i()
{
    word_t  i = return_stack.top();
    integer_stack.push(i);
    return neutral;
}

state_t index_j()
{
    word_t  i = return_stack.top();

    return_stack.pop();
    word_t  i_max = return_stack.top();
    return_stack.pop();
    word_t  j = return_stack.top();
    return_stack.push(i_max);
    return_stack.push(i);

    integer_stack.push(j);
    return neutral;
}

state_t op_return()
{
    word_t r = return_stack.top();
    return_stack.pop();
    memory.jump(r);
    return neutral;
}

state_t do_condition()
{
    word_t condition = integer_stack.top();
    integer_stack.pop();
    word_t a = memory.get();
    if (condition == 0)
    {
        memory.jump(a);
    }
    return neutral;
}

state_t do_allot()
{
    state_t state;
    // debug
    word_t ca = memory.get_current_address();
    word_t ea = memory.get_execution_address();
    ca = memory.get(ca);
    ea = memory.get(ea);
    record_t* ea_record = &words[ea];
    record_t* ca_record = &words[ca];
    if(memory.may_run())
        state = execute();

    word_t count = integer_stack.top();
    integer_stack.pop();
    if (count & 0x3) {
        printf("TODO: fix variable alignment");
        return error;
    }
    count >>= 2;
    ca = memory.get_current_address() + count;
    memory.jump(ca);
    memory.update_execution_address();
    return neutral;
}

#include <algorithm>
#include <cctype>

state_t check_item(std::string& item, state_t state)
{
    uint32_t n, a, b;
    uint32_t h = hash(item.c_str());

    switch (h)
    {
        case hash(":"):
            state = function_name;
            break;
        case hash(";"):
            memory.put(h);
            func_name.clear();
            memory.update_execution_address();
            state = neutral;
            break;
        case hash("if"):
            memory.put(h);
            n = memory.get_current_address();
            condition_stack.push(n);
            memory.put(0);
            break;
        case hash("else"):
            n = hash("^jump");
            memory.put(n);
            b = memory.get_current_address();
            memory.put(0);
            a = memory.get_current_address();
            n = condition_stack.top();
            condition_stack.pop();
            condition_stack.push(b);
            memory.set(n, a);
            break;
        case hash("then"):
            a = memory.get_current_address();
            n = condition_stack.top();
            condition_stack.pop();
            memory.set(n, a);
            break;
        case hash("do"):
        case hash("begin"):
            prepare_enter_loop(h);
            break;
        case hash ("leave"):
            prepare_leave();
            break;
        case hash("until"):
        case hash("loop"):
        case hash("+loop"):
        case hash("again"):
            prepare_loop_exit(h);
            break;
        case hash("variable"):
            state = variable_state;
            break;
        case hash("constant"):
            state = constant_value;;
            break;
        case hash("allot"):
            state = do_allot();
            break;
        case hash(".\""):
            state = print_string;
            break;
        case hash("\\"):
            state = eol_comment;
            break;
        case hash("("):
            state = forth_comment;
            break;
        case hash("bye"):
            state = finish;
            break;
        case hash("hex"):
            memory.put(h);
            if (func_name.empty())
                memory.set(base_address, 16);
            break;
        case hash("decimal"):
            memory.put(h);
            if(func_name.empty())
                memory.set(base_address, 10);
            break;
        case hash("octal"):
            memory.put(h);
            if (func_name.empty())
                memory.set(base_address, 8);
            break;
        default:
        {
            if (words.count(h) == 0)
            {
                std::string lowered = item;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                uint32_t lower_h = hash(lowered.c_str());
                if (words.count(lower_h) > 0) {
                    record_t* r = &words[lower_h];
                    if (r->TYPE == builtin || r->TYPE == variable || r->TYPE == user)
                    {
                        h = lower_h;
                    }
                    else {
                        printf("Warn: case conversion case word %s is %s\n", lowered.c_str(), item.c_str());
                    }
                }

            }

            if (words.count(h) > 0)
            {
                record_t* r = &words[h];
                switch (r->TYPE) {
                case builtin:
                case user:
                    memory.put(h);
                    break;
                case constant:
                    memory.put(hash("^push"));
                    memory.put(r->CONSTANT);
                    break;
                case variable:
                    memory.put(hash("^push"));
                    memory.put(r->ADDR);
                    break;
                default:
                    return error;
                }
            }
            else
            {
                char ch = item[0];
                if (ch > 0)
                {
                    if (isdigit(ch) || ch == '-') {
                        state = parse_number(item);
                        break;
                    }
                    if (ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F') {
                        state = parse_number(item);
                        break;
                    }
                }
                printf("Line: %d Undefined name: %s\n", line_no, item.c_str());
                state = error;
            }
        }
        break;
    }
    item.clear();
    return state;
}

#define CLEAN_AFTER_RUN

state_t execute()
{
    word_t  r;
#ifdef CLEAN_AFTER_RUN
    word_t start = memory.get_current_address();
#endif
    while (memory.running())
    {
#ifdef TRACE
        a = memory.get_current_address();
#endif
        word_t cmd = memory.get();

        if (words.count(cmd) == 0) {
            printf("\nCommand not implemented\n");
            return error;
        }
        
        record_t* rec = &words[cmd];
#ifdef TRACE
            printf("\n%08x: %s ", a, rec->NAME.c_str());
#endif
            if ( func_name.empty() && rec->MIN_STACK > integer_stack.size()) {
                char err_buff[80];
                snprintf(err_buff, 80, "%s[%u]: Stack underflow operation '%s'",
                    func_name.c_str(),
                    memory.get_current_address(),
                    rec->NAME.c_str()
                    );
                r = memory.get_execution_address();
                memory.jump(r);
                throw std::out_of_range(std::string(err_buff));
            }

        switch (rec->TYPE)
        {
        case builtin:
            rec->BLTN();
            break;
        case user:
            r = memory.get_current_address();
            return_stack.push(r);
            memory.jump(rec->ADDR);
            break;
        case constant:
            throw std::runtime_error("Constant data has no eXecute rights");
            break;
        case variable:
            throw std::runtime_error("Variable has no eXecute rights");
            break;
        default:
            throw std::runtime_error("Non implemented command type");
        }

    }
#ifdef CLEAN_AFTER_RUN
    word_t stop = memory.get_finish();
    for (int i = start; i < stop; i++)
    {
        memory.set(i, 0);
    }
#endif
    return neutral;
}

state_t forth(const char* str)
{
    int ch;
    std::string     buffer;
    int status = 0;
    state_t state = neutral;
    ++line_no;

    for (; state != error && state != done && state != finish;)
    {
        ch = *str++;
        switch (state)
        {
        case neutral:
            switch (ch)
            {
            case 0:
            case '\n':
            case '\t':
            case ' ':
                if (!buffer.empty()) {
#if TRACE
                    printf("TRACE %s\n", buffer.c_str());
#endif
                    state = check_item(buffer, state);
                }
                if (state == error || state == done)
                    continue;
                if (func_name.empty() && (ch == '\n' || ch == 0)) {
                    if (memory.may_run())
                        state = execute();
                }
                if (ch == 0)
                    state = done;
                continue;

            default:
                buffer += ch;
            }
            continue;

        case eol_comment:
            switch (ch)
            {
            case '\n':
                state = neutral;
                break;
            case 0:
                if (memory.may_run())
                    state = execute();
                if(state != error)
                    state = done;
            }
            continue;

        case forth_comment:
            if (ch == ')')
                state = neutral;
            continue;

        case function_name:
            if (ch != ' ' && ch != '\n') {
                buffer += ch;
            }
            else {
                 func_name = buffer;
                buffer.clear();
                word_t  position = memory.get_current_address();
                uint32_t h = register_word(func_name, position);
                memory.put(hash(":"));
                memory.put(h);
                state = neutral;
            }
            continue;

        case variable_state:
            switch (ch)
            {
            case 0:
            case '\n':
            case ' ':
                if (!buffer.empty())
                    state = register_variable(buffer);
                if (ch == 0)
                    state = done;
                buffer.clear();
                continue;
            default:
                buffer += ch;
            }
            continue;

        case constant_value:
            switch(ch)
            {
            case 0:
            case '\n':
            case ' ':
                if (!buffer.empty())
                    state = register_constant(buffer);
                if (ch == 0)
                    state = done;
                buffer.clear();
                continue;
            default:
                buffer += ch;
            }
            continue;

        case print_string:
            if (ch == '\"') {
                memory.put(hash(".\""));
                memory.put(hash("^jump"));
                word_t a = memory.get_current_address();
                memory.put(0);
                memory.put(buffer.length());
                int wc = (buffer.length() + 3) / 4; // Only for 32-bit system
                word_t* ptr = (word_t*) buffer.c_str();
                for (int i = 0; i < wc; i++)
                    memory.put(*ptr++);
                memory.set(a, memory.get_current_address());
                buffer.clear();
                state = neutral;
                continue;
            }
            buffer += ch;
            continue;
            
//        case finish:
            

        default:
            puts("Unknown state");
            state = error;
            status = -1;
        }
    }
    return state;
}

state_t   register_constant(std::string& word)
{
    state_t state = error;
    uint32_t h = hash(word.c_str());
    record_t    rec;

    if (words.count(h) > 0) {
        printf("Constant %s already defined\n", word.c_str());
        exit(-1);
    }

    if (memory.may_run())
        state = execute();

    rec.INDEX = words.size();
    rec.NAME = word;
    rec.TYPE = constant;
    rec.CONSTANT = integer_stack.top();
    rec.MIN_STACK = 0;
    rec.GENERATE = nullptr;
    integer_stack.pop();
    words[h] = rec;
    define_constant(&rec);
    return state;
}

state_t   register_variable(std::string &word)
{
    uint32_t h = hash(word.c_str());
    record_t    rec;

    if (words.count(h) > 0) {
        printf("Variable %s already defined\n", word.c_str());
        exit(-1);
    }

    uint32_t ea = memory.get_execution_address();
    uint32_t a = memory.get_current_address();
    //if (ea != a)
    //{
    //    // Это не сработает для allot
    //    memory.put(hash("^jump"));
    //    word_t a = memory.get_current_address();
    //    memory.put(a + 3);
    //    throw "Insert jmp";
    //}
    rec.INDEX = words.size();
    rec.NAME = word;
    rec.TYPE = variable;
    rec.MIN_STACK = 0;
    rec.ADDR = memory.get_current_address();
    words[h] = rec;
    memory.put(0);
    memory.update_execution_address();
    define_global_variable(&rec);
    return neutral;
}

uint32_t    register_word(std::string &word, word_t address)
{
    uint32_t h = hash(word.c_str());
    record_t    rec;

    if (words.count(h) > 0) {
        printf("Word %s already defined\n", word.c_str());
        exit(-1);
    }

    rec.INDEX = words.size();
    rec.NAME = word;
    rec.TYPE = user;
    rec.ADDR = address;
    rec.MIN_STACK = 0;
    rec.GENERATE = nullptr;
    words[h] = rec;
    define_function(&rec);
    return h;
}

state_t   register_builtin(std::string name, int min_stack, code_ptr_t code, genetator_t generator)
{
    uint32_t h = hash(name.c_str());
    record_t    rec;

    if (words.count(h) > 0) {
        printf("Word %s already defined\n", name.c_str());
        exit(-1);
    }

    rec.NAME = name;
    rec.TYPE = builtin;
    rec.BLTN = code;
    rec.GENERATE = generator;
    rec.MIN_STACK = min_stack;
    words[h] = rec;
    register_code(&rec);
    return neutral;
}

void register_variable(const char * variable_name)
{
    std::string variable(variable_name);
    register_variable(variable);
}

void register_variable(const char * variable_name, int value)
{
    register_variable(variable_name);
    uint32_t h = hash(variable_name);
    record_t*    prec;

    prec = &words[h];
#if TRACE
    printf("Set variable: %s ADDR %u = %d\n", variable_name, prec->ADDR, value);
#endif
    memory.set(prec->ADDR, value);
}

void decompile()
{
    memory.jump(0);
    while (memory.running())
    {
        word_t a = memory.get_current_address();

        word_t cmd = memory.get();

        if (words.count(cmd) == 0) {
            printf("[%u]: %d\n", a, cmd);
            continue;
        }
        record_t* rec = &words[cmd];
        switch (rec->TYPE)
        {
        case builtin:
            printf("[%u]: %s BUILTIN\n", a, rec->NAME.c_str());
            break;
        case user:
            printf("[%u]: %s USER\n", a, rec->NAME.c_str());
            break;
        case constant:
            printf("[%u]: %s CONSTANT\n", a, rec->NAME.c_str());
            break;
        case variable:
            printf("[%u]: %s VARIABLE\n", a, rec->NAME.c_str());
            break;
        default:
            printf("[%u]: BAD TYPE\n", a);
            break;
        }
    }
}

state_t init()
{
    try {
        register_variable("base", 10);
        uint32_t h = hash("base");
        record_t* r = &words[h];
        base_address = r->ADDR;

        //        register_variable("num-format", 0);
        register_variable("ansi-term");

        register_builtin(":", 0, register_function, asm_function);
        register_builtin("^push", 0, push_value, asm_push_value);
        register_builtin("^jump", 0, do_jump, asm_jump);
        register_builtin("here", 0, do_here, asm_here);
        register_builtin("!", 2, do_store, asm_store);
        register_builtin("@", 1, do_fetch, asm_fetch);
        register_builtin("+", 2, do_add, asm_add);
        register_builtin("-", 2, do_sub, asm_sub);
        register_builtin("*", 2, do_mul, asm_mul);
        register_builtin("/", 2, do_div, asm_div);
        register_builtin("=", 2, check_eq, asm_check_equ);
        register_builtin("<>", 2, check_not_eq, asm_check_notequ);
        register_builtin("<", 2, check_less, asm_check_less);
        register_builtin(">", 2, check_more, asm_check_more);
        register_builtin("and", 2, op_and, asm_and);
        register_builtin("or", 2, op_or, asm_or);
        register_builtin("xor", 2, op_xor, asm_xor);
        register_builtin("invert", 1, invert, asm_not);
        register_builtin("mod", 2, mod, asm_mod);
        register_builtin("/mod", 2, divmod, asm_divmod);
        register_builtin("rshift", 2, op_rshift, asm_right_shift);
        register_builtin("lshift", 2, op_lshift, asm_left_shift);

        register_builtin(">r", 1, to_R, asm_to_R);
        register_builtin("r>", 0, from_R, asm_from_R);
        register_builtin("r@", 0, fetch_R, asm_fetch_R);

        register_builtin("dup", 1, dup, asm_dup);
        register_builtin("drop", 1, drop, asm_drop);
        register_builtin("swap", 2, swap, asm_swap);
        register_builtin("over", 2, over, asm_over);
        register_builtin("rot", 3, rot, asm_rot);
        register_builtin(";", 0, op_return, asm_return);

        register_builtin("if", 1, do_condition, asm_if);

        register_builtin("do", 2, do_loop, asm_loop);
        register_builtin("loop", 0, end_loop, asm_endloop);
        register_builtin("+loop", 1, end_plus_loop, asm_endplusloop);
        register_builtin("^leave", 0, do_leave, asm_leave);
        register_builtin("i", 0, index_i, asm_index_i);
        register_builtin("j", 0, index_j, asm_index_j);

        register_builtin("begin", 0, do_begin, asm_beginloop);
        register_builtin("until", 1, do_until, asm_untilloop);
        register_builtin("again", 0, do_again, asm_again);

        register_builtin(".", 1, do_dot, asm_dot);
        register_builtin(".\"", 0, do_print_string, asm_print_string);
        register_builtin("emit", 1, emit, asm_emit);
        register_builtin("key", 0, get_key, asm_key);
        register_builtin("words", 0, show_words, nullptr);

        // Defaults
        forth("0 ansi-term !");
        forth(": cr 10 emit ;");
        forth(": cells 4 * ;");
        forth(": ? @ . ;");
        forth(": decimal  10 base ! ;");
        forth(": hex      16 base ! ;");
        forth(": octal     8 base ! ;");
        line_no = 0;
    }
    catch (std::exception const& a)
    {
        fprintf(stderr, "init() exception: %s\n", a.what());
    }
    return neutral;
}

void generate_asm_code(const char* src)
{
//    decompile(); read_key();
    void generate_code(const char*, ram_memory & memory, registry_t & words);
    generate_code(src, memory, words);
}

