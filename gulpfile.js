const gulp = require('gulp');
const pump = require('pump');
const uglify = require('gulp-uglify');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const gzip = require('gulp-gzip');

const htmlSrc = "www/*.html";
const jsSrc = "www/*.js";
const cssSrc = "www/*.css";
const minDest = "dest/min";
const gzipDest = "dest/gzip";

gulp.task('minify-html', function (cb) {
    pump([
            gulp.src(htmlSrc),
            htmlmin({
                collapseWhitespace: true
            }),
            gulp.dest(minDest)
        ],
        cb
    );
});

gulp.task('minify-css', function (cb) {
    pump([
            gulp.src(cssSrc),
            cleancss(),
            gulp.dest(minDest)
        ],
        cb
    );
});

gulp.task('minify-js', function (cb) {
    pump([
            gulp.src(jsSrc),
            uglify({
                compress: {
                    'drop_console': false
                }
            }),
            gulp.dest(minDest)
        ],
        cb
    );
});

gulp.task('gzip-all', function (cb) {
    pump([
            gulp.src(minDest + "/*"),
            gzip({
                append: true
            }),
            gulp.dest(gzipDest)
        ],
        cb
    );
});

gulp.task('minify', gulp.series('minify-html', 'minify-css', 'minify-js'));
gulp.task('gzip', gulp.series('minify', 'gzip-all'));
gulp.task('default', gulp.series('gzip'));
