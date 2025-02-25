#include <time.h>
#include <termios.h>
#include <signal.h>
#include <locale.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/hid/IOHIDLib.h>
#include <ApplicationServices/ApplicationServices.h>

#define END 999999

static struct termios original_termios;

static pthread_mutex_t load_mutex = PTHREAD_MUTEX_INITIALIZER;
static float target_load = 0.5f;
static float current_load = 0.5f;
static atomic_bool should_exit = false;

void nsleep(uint64_t nanoseconds) {
    mach_wait_until(mach_absolute_time() + nanoseconds);
}

bool is_caps_lock_key_active() {
    CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
    return flags & kCGEventFlagMaskAlphaShift;
}


void* cpu_load(void* arg) {
    int core = *(int*)arg;
    
    thread_port_t thread = pthread_mach_thread_np(pthread_self());
    thread_affinity_policy_data_t policy = { core };
    thread_policy_set(thread, THREAD_AFFINITY_POLICY, 
                    (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);

    const uint64_t interval = 10000000;
    uint64_t start_time, current_time;

    while(!atomic_load(&should_exit)) {
        pthread_mutex_lock(&load_mutex);
        float load = current_load;
        pthread_mutex_unlock(&load_mutex);

        uint64_t busy_time = interval * load;
        start_time = mach_absolute_time();
        
        do {
            current_time = mach_absolute_time();
        } while(current_time - start_time < busy_time);
        
        nsleep(interval - busy_time);
    }
    
    return NULL;
}

void* load_controller(__attribute__((unused)) void* arg) {
    const uint64_t update_interval = 150000;
    const float step = (float) rand() / RAND_MAX * 0.008 + 0.001;
    long km = 0;
    
    while(!atomic_load(&should_exit)) {
        bool caps_enabled = is_caps_lock_key_active();
        
        pthread_mutex_lock(&load_mutex);
        if(caps_enabled) {
            target_load += step;
            if(target_load > 1.) target_load = 1.;
        } else {
            target_load -= step;
            if(target_load < .0) target_load = .0;
        }
        
        current_load += (target_load - current_load) * .1;
        if (current_load < .1 || current_load > .9 || km >= END) {
            atomic_store(&should_exit, true);
        }

        pthread_mutex_unlock(&load_mutex);

        if (km < END) {
            printf("\e[30;47;1m\r  %06ld  \e[0m", ++km);
        } else {
            printf("\e[30;47;1m\r YOU WON! \e[0m");
        }
        fflush(stdout);
        
        nsleep(update_interval);
    }
    
    return NULL;
}

void restore_cursor(__attribute__((unused)) int sig) {
    printf("\e[?25h\e[0m\e\n\n");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    exit(0);
}

void prepare_console() {
    struct sigaction sa = { .sa_handler = restore_cursor, .sa_flags = 0 };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios new_termios = original_termios;

    new_termios.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

    printf("\e[?25l");
}

void print_counter() {
    for (size_t i = 0; i < 3; i++) printf("\e[47m          \e[0m\n");
    printf("\e[2F");

    setlocale(LC_ALL, "");
    const char *symbols[] = {"🟢🟢🟢", "🔴🟢🟢", "🔴🔴🟢", "🔴🔴🔴"};
    for (size_t i = 0; i < 4; i++) {
        printf("\e[47m\r  %s  \e[0m", symbols[i]);
        fflush(stdout);
        sleep(1);
    }

    printf("\e[0m");
}

void check_keyboard_permission() {
    if (!AXIsProcessTrusted()) {
        printf("This app requires accessibility permissions to monitor keyboard input.\n");
        printf("Please enable it in System Preferences → Security & Privacy → Accessibility.\n");

        CFStringRef keys[] = { kAXTrustedCheckOptionPrompt };
        CFBooleanRef values[] = { kCFBooleanTrue };
        CFDictionaryRef options = CFDictionaryCreate(
            NULL, (const void **)keys, (const void **)values, 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
        );

        AXIsProcessTrustedWithOptions(options);
        CFRelease(options);
    }
}

int main() {
    check_keyboard_permission();

    srand(time(NULL));

    prepare_console();
    print_counter();

    long core_count = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[core_count];
    long core_ids[core_count];
    
    pthread_t controller_thread;
    pthread_create(&controller_thread, NULL, load_controller, NULL);

    for(long i = 0; i < core_count; i++) {
        core_ids[i] = i;
        pthread_create(&threads[i], NULL, cpu_load, &core_ids[i]);
    }

    for(long i = 0; i < core_count; i++) {
        pthread_join(threads[i], NULL);
    }
 
    restore_cursor(0);
    return 0;
}
