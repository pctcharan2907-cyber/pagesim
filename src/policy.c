#include "policy.h"
#include <string.h>
#include <ctype.h>

static PageReplacementPolicy *all_policies[] = {
    NULL, /* policy_fifo */
    NULL, /* policy_lru */
    NULL, /* policy_clock */
    NULL, /* policy_optimal */
    NULL  /* policy_aging */
};

static void ensure_policies_initialized(void)
{
    all_policies[0] = policy_fifo_get();
    all_policies[1] = policy_lru_get();
    all_policies[2] = policy_clock_get();
    all_policies[3] = policy_optimal_get();
    all_policies[4] = policy_aging_get();
}

int policy_get_count(void)
{
    return 5;
}

PageReplacementPolicy *policy_get_by_index(int index)
{
    ensure_policies_initialized();
    if (index >= 0 && index < 5) {
        return all_policies[index];
    }
    return NULL;
}

static bool str_case_eq(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return false;
        }
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

PageReplacementPolicy *policy_find_by_name(const char *name)
{
    if (!name) return NULL;
    ensure_policies_initialized();

    for (int i = 0; i < 5; i++) {
        if (str_case_eq(name, all_policies[i]->name)) {
            return all_policies[i];
        }
    }

    /* Aliases */
    if (str_case_eq(name, "opt") || str_case_eq(name, "min")) {
        return all_policies[3]; /* Optimal */
    }
    if (str_case_eq(name, "second-chance") || str_case_eq(name, "clk")) {
        return all_policies[2]; /* Clock */
    }
    if (str_case_eq(name, "nfu")) {
        return all_policies[4]; /* Aging */
    }

    return NULL;
}

void policy_print_all(void)
{
    ensure_policies_initialized();
    printf("\nAvailable Page Replacement Policies:\n");
    printf("----------------------------------------------------------------------\n");
    for (int i = 0; i < 5; i++) {
        PageReplacementPolicy *p = all_policies[i];
        printf("  %-10s [%s] - %s\n", p->name, p->is_stack_algorithm ? "Stack Alg" : "Non-Stack", p->description);
    }
    printf("----------------------------------------------------------------------\n");
}
