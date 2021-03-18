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
                test: /\.js$/u,
                exclude: /node_modules/u,
                use: {
                    loader: "babel-loader"
                }
            },
            {
                test: /\.css$/u,
                loaders: [MiniCssExtractPlugin.loader, 'css-loader']
            }
        ]
    },
    optimization: {
        minimize: true,
        minimizer: [
            new TerserPlugin({
                parallel: true,
                test: /\.js(?<x>\?.*)?$/iu,
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
        new OptimizeCSSAssetsPlugin({}),
        new CompressionPlugin(),
        new CopyPlugin({
            patterns: [
                { from: 'www/lib', to: 'gzip' },
                { from: 'www/lib', to: '' }
            ],
        })
    ],
    devServer: {
        port: 3000,
        contentBase: './dist'
    }
}