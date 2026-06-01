#include <string.h>
#include <stddef.h>

#define ENV_MAX 64
#define ENV_BUF_SIZE 4096

static char *env_ptrs[ENV_MAX + 1];
static char env_buf[ENV_BUF_SIZE];
static int env_used;
static int env_count;
char **environ = env_ptrs;

void __env_init(char **envp)
{
	int i;
	env_used = 0;
	env_count = 0;
	if (!envp)
		return;
	for (i = 0; envp[i] && env_count < ENV_MAX; i++) {
		int len = 0;
		while (envp[i][len])
			len++;
		len++;
		if (env_used + len > ENV_BUF_SIZE)
			break;
		memcpy(env_buf + env_used, envp[i], len);
		env_ptrs[env_count++] = env_buf + env_used;
		env_used += len;
	}
	env_ptrs[env_count] = 0;
}

static int env_find_name(const char *entry, const char *name, int nlen)
{
	int i;
	for (i = 0; i < nlen; i++) {
		if (entry[i] != name[i])
			return 0;
	}
	return entry[i] == '=';
}

char *getenv(const char *name)
{
	int i, nlen;
	if (!name)
		return 0;
	nlen = 0;
	while (name[nlen])
		nlen++;
	for (i = 0; env_ptrs[i]; i++) {
		if (env_find_name(env_ptrs[i], name, nlen))
			return env_ptrs[i] + nlen + 1;
	}
	return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
	int i, nlen, vlen, total_len;
	if (!name || !value || name[0] == '\0')
		return -1;
	for (i = 0; name[i]; i++) {
		if (name[i] == '=')
			return -1;
	}
	nlen = 0;
	while (name[nlen])
		nlen++;
	vlen = 0;
	while (value[vlen])
		vlen++;
	for (i = 0; env_ptrs[i]; i++) {
		if (!env_ptrs[i])
			continue;
		if (env_find_name(env_ptrs[i], name, nlen)) {
			if (!overwrite)
				return 0;
			int old_vlen = 0;
			char *old_val = env_ptrs[i] + nlen + 1;
			while (old_val[old_vlen])
				old_vlen++;
			if (vlen <= old_vlen) {
				memcpy(old_val, value, vlen);
				old_val[vlen] = '\0';
				return 0;
			}
			for (; env_ptrs[i]; i++)
				env_ptrs[i] = env_ptrs[i + 1];
			env_count--;
			break;
		}
	}
	total_len = nlen + 1 + vlen + 1;
	if (env_count >= ENV_MAX || env_used + total_len > ENV_BUF_SIZE)
		return -1;
	memcpy(env_buf + env_used, name, nlen);
	env_buf[env_used + nlen] = '=';
	memcpy(env_buf + env_used + nlen + 1, value, vlen);
	env_buf[env_used + nlen + 1 + vlen] = '\0';
	env_ptrs[env_count++] = env_buf + env_used;
	env_ptrs[env_count] = 0;
	env_used += total_len;
	return 0;
}

int putenv(char *string)
{
	int i, nlen, len;
	if (!string)
		return -1;
	for (nlen = 0; string[nlen]; nlen++) {
		if (string[nlen] == '=')
			break;
	}
	if (string[nlen] != '=')
		return -1;
	for (i = 0; env_ptrs[i]; i++) {
		if (!env_ptrs[i])
			continue;
		if (env_find_name(env_ptrs[i], string, nlen)) {
			for (; env_ptrs[i]; i++)
				env_ptrs[i] = env_ptrs[i + 1];
			env_count--;
			break;
		}
	}
	len = nlen + 1;
	while (string[len])
		len++;
	len++;
	if (env_count >= ENV_MAX || env_used + len > ENV_BUF_SIZE)
		return -1;
	memcpy(env_buf + env_used, string, len);
	env_ptrs[env_count++] = env_buf + env_used;
	env_ptrs[env_count] = 0;
	env_used += len;
	return 0;
}

int clearenv(void)
{
	env_used = 0;
	env_count = 0;
	env_ptrs[0] = 0;
	return 0;
}
