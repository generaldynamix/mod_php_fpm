# mod_php_fpm

## English

### Summary

mod_php_fpm is an apache 2.x module to parse mod_php .htaccess variables (e.g. php_value/php_flag) to PHP-CGI/FCGI. Licensed under Apache 2.0 License. Code based on standard apache mod_env.

Module is intended to use at shared webhosting servers, where .htaccess files widely used, and giving clients full access to Apache config files (or editing a php-fpm pool configs) is not acceptable.

### Compilation

You must have apache2 development package, apxs2 and gcc/binutils/etc. installed, e.g. (on Ubuntu/Debian)
`apt install apache2-dev build-essential`

Then, compile module and copy it into apache2 module dir (e.g. `/usr/lib/apache2/modules`)

`apxs -c mod_php_fpm.c`

`cp .libs/mod_php_fpm.so /where/is/your/apache/modules`

### Apache setup

Just add `LoadModule php_fpm_module /usr/lib/apache2/modules/mod_php_fpm.so` to your apache2 config (either in apache2.conf or via mods-available/mods-enabled dirs) and restart apache. Feel free to add PHP configuration directives in .htaccess files now.

Remember - php_admin_value and php_admin_flag variables are to set up only in server configuration files, they cannot be set in .htaccess

Example of test .htaccess file:

```
php_value error_reporting -1
php_flag log_errors 0
php_flag allow_url_fopen Off
```

