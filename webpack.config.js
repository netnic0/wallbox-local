const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const TerserPlugin = require('terser-webpack-plugin');
const CompressionPlugin = require('compression-webpack-plugin');
const HtmlWebpackInlineSourcePlugin = require('html-webpack-inline-source-plugin');
const { CleanWebpackPlugin } = require('clean-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const OptimizeCSSAssetsPlugin = require('optimize-css-assets-webpack-plugin');
const CopyPlugin = require('copy-webpack-plugin');

module.exports = {
    entry: './www/app/app.js',
    output: {
        path: path.resolve(__dirname, 'dist'),
        filename: 'bundle.min.js'
    },
    module: {
        rules: [
            {
                test: /\.js$/,
                exclude: /node_modules/,
                use: {
                    loader: "babel-loader"
                }
            },
            {
                test: /\.css$/,
                loaders: [MiniCssExtractPlugin.loader, 'css-loader']
            }
        ]
    },
    optimization: {
        minimize: true,
        minimizer: [
            new TerserPlugin({
                parallel: true,
                test: /\.js(\?.*)?$/i,
            }, new OptimizeCSSAssetsPlugin({})),
        ],
    },
    plugins: [
        new CleanWebpackPlugin(),
        new HtmlWebpackPlugin({
            inlineSource: '.(js|css)$', // embed all javascript and css inline,
            template: 'www/index.html'
        }),
        new HtmlWebpackInlineSourcePlugin(HtmlWebpackPlugin),
        new MiniCssExtractPlugin({
            filename: '[name].css',
            chunkFilename: '[id].css',
        }),
        new CompressionPlugin(),
        new CopyPlugin({
            patterns: [
                { from: 'www/lib', to: 'gzip' },
                { from: 'www/lib', to: '' },
                { from: 'dist/*.html.gz', to: 'gzip', flatten: true },
            ],
        })
    ],
    devServer: {
        port: 3000,
        contentBase: './dist'
    }
}