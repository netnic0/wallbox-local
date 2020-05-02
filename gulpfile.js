const gulp = require('gulp');
const pump = require('pump');
const uglify = require('gulp-uglify');
const htmlmin = require('gulp-htmlmin');
const mincss = require("gulp-minify-css");
const gzip = require('gulp-gzip');

const htmlSrc = "www/*.html";
const jsSrc = "www/*.js";
const cssSrc = "www/*.css";
const minDest = "dest/min";
const gzipDest = "dest/gzip";

gulp.task('minhtml', function (cb) {
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

gulp.task('mincss', function (cb) {
    pump([
            gulp.src(cssSrc),
            mincss(),
            gulp.dest(minDest)
        ],
        cb
    );
});

gulp.task('minjs', function (cb) {
    pump([
            gulp.src(jsSrc),
            uglify({
                compress: {
                    'drop_console': true
                }
            }),
            gulp.dest(minDest)
        ],
        cb
    );
});

gulp.task('gzipall', function (cb) {
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

gulp.task('min', gulp.series('minjs', 'mincss', 'minhtml'));
gulp.task('gzip', gulp.series('minjs', 'mincss', 'minhtml', 'gzipall'));
gulp.task('default', gulp.series('gzip'));
