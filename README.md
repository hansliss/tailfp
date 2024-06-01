# tailfp
A simple tail -f hack that can keep track of multiple files and open new ones by glob pattern

Example:
```
tailfp "/opt/foo/logs/2*.log"
```

Don't forget to escape the glob pattern to ensure that your shell doesn't expand it!

